/*
 *  Copyright (C) 2014 David Liang
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#endif

#include "ev-dc.h"

enum {
	TAG_ALL,
	TAG_UNREAD,
	TAG_READING,
	TAG_FINISHED,
	TAG_END
};

	
const gchar *tag_labels[5] = {
	    N_("All"),
	    N_("Unread"),
	    N_("Reading"),
	    N_("Finished"),
	    NULL
};

enum {
	CHANGED,
        ACTIVATED,

	N_SIGNALS
};

static guint signals[N_SIGNALS] = {0, };

struct _EvDCPrivate {
	GnomeDesktopThumbnailFactory *gf;
        GHashTable	*tag_table;
        GHashTable	*thumbail_table;
	GList		*monitor_dirs;
	GList           *pdf_files;
	GList		*remote_files;
	gchar 		*current_file;

        GPid            list_child_pid;
        guint           list_child_watch_id;

        pid_t           child_pid;
        guint           child_watch_id;

	GdkPixbuf 	*file_pixbuf;
	GtkWidget	*header_bar;
	gint		current_header_view;
	GtkWidget	*header_view;
	GtkWidget	*header_tag_button;
	GtkWidget	*header_sync_button;
	GtkWidget	*header_share_button;
	GtkWidget 	*header_remote_file_button;
	GtkWidget	*sw; 
	GtkWidget 	*icon_view;
	GtkListStore 	*store;
};

G_DEFINE_TYPE (EvDC, ev_dc, GTK_TYPE_BOX)

#define FILE_NAME "/usr/share/icons/hicolor/32x32/apps/flash-player-properties.png"

enum
{
	COL_PATH,
	COL_DISPLAY_NAME,
	COL_PIXBUF,
	NUM_COLS
};

static void	save_tags (EvDC *dc);
static void	load_tags (EvDC *dc);
static void	load_icon_view (EvDC *dc);

static void
load_pdf_dirs (EvDC *dc)
{
	/*TODO: make add other dir? online dir? */
	dc->priv->monitor_dirs = g_list_append (dc->priv->monitor_dirs, g_strdup (g_get_home_dir ()));
}

static void
load_pdfs (gchar *parent, EvDC *dc)
{
	GDir *dir;
	const gchar *name;

	dir = g_dir_open (parent, 0, NULL);
	if (!dir)
	  	return;

        while ((name = g_dir_read_name (dir))) {
		gchar *path;
		gboolean is_dir;
		/* We ignore hidden files that start with a '.' */
		if (name[0] != '.') {
			path = g_build_filename (parent, name, NULL);
	    		is_dir = g_file_test (path, G_FILE_TEST_IS_DIR);
			if (is_dir) {
				load_pdfs (path, dc);
			} else if (g_str_has_suffix (path, ".pdf")) {
				dc->priv->pdf_files = g_list_prepend (dc->priv->pdf_files, g_strdup (path));
			}
	    		g_free (path);
		}
	}

	g_dir_close (dir);
}

static void
load_pdf_files (EvDC *dc)
{
	GList *l;

	for (l = dc->priv->monitor_dirs; l; l = l->next) {
		load_pdfs ((gchar *)(l->data), dc);
	}
}

static gboolean
file_fit_view (EvDC *dc, const gchar *path)
{
	gchar *value;
	gint tag;

        value = g_hash_table_lookup (dc->priv->tag_table, path);

	if (value) {
		tag = atoi (value);
	} else {
		tag = TAG_UNREAD;
	}
	if (dc->priv->current_header_view == TAG_ALL) {
		return TRUE;
	} else if (tag == dc->priv->current_header_view) {
		return TRUE;
	} else
		return FALSE;
}

static GdkPixbuf *
get_thumbail (EvDC *dc, const char *path)
{
	gchar *thumbail_path;
	GdkPixbuf *thumbail_pixbuf;
		
	thumbail_pixbuf = g_hash_table_lookup (dc->priv->thumbail_table, path);
	if (thumbail_pixbuf) {
		return thumbail_pixbuf;
	}

	thumbail_path = gnome_desktop_thumbnail_path_for_uri (path, GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	thumbail_pixbuf = gdk_pixbuf_new_from_file(thumbail_path, NULL);
	g_free (thumbail_path);
		
	if (!thumbail_pixbuf) {
		thumbail_pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (dc->priv->gf, path, "application/pdf");
		gnome_desktop_thumbnail_factory_save_thumbnail (dc->priv->gf, thumbail_pixbuf, path, 0);
	}

	if (!thumbail_pixbuf)
		thumbail_pixbuf = dc->priv->file_pixbuf;

	g_hash_table_insert (dc->priv->thumbail_table, (gpointer) path, (gpointer) thumbail_pixbuf);

	return thumbail_pixbuf;
}

static void
load_icon_view (EvDC *dc)
{
	GtkListStore *store;
	gchar *display_name;
	const gchar *path;
	GList *pdf_files, *l;
	GtkTreeIter iter;

	store = dc->priv->store;

	pdf_files = dc->priv->pdf_files;
	/* First clear the store */
	gtk_list_store_clear (store);

	for (l = pdf_files; l; l = l->next) {
		path = (gchar *) l->data;
		if (!file_fit_view (dc, path)) {
			continue;
		}

		GdkPixbuf *thumbail_pixbuf;
		thumbail_pixbuf = get_thumbail (dc, path);
		display_name = g_path_get_basename (path);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
	                        COL_PATH, path,
	                        COL_DISPLAY_NAME, display_name,
	                        COL_PIXBUF, thumbail_pixbuf,
	                        -1);
		g_free (display_name);
	}
}

static gint
sort_func (GtkTreeModel *model,
	     GtkTreeIter  *a,
	     GtkTreeIter  *b,
	     gpointer      user_data)
{
	gchar *a_path, *b_path;
        time_t a_atime, b_atime;

	GFile *a_file, *b_file;
	GFileInfo *a_info, *b_info;
	/* sort by access time */
	/* TODO maybe recent file ? */
	gtk_tree_model_get (model, a,
			COL_PATH, &a_path,
	                -1);

	gtk_tree_model_get (model, b,
			COL_PATH, &b_path,
	                -1);

	a_file = g_file_new_for_path (a_path);
	b_file = g_file_new_for_path (b_path);

	a_info = g_file_query_info (a_file, G_FILE_ATTRIBUTE_TIME_ACCESS, 0, NULL, NULL);
	b_info = g_file_query_info (b_file, G_FILE_ATTRIBUTE_TIME_ACCESS, 0, NULL, NULL);

       	a_atime = g_file_info_get_attribute_uint64 (a_info, G_FILE_ATTRIBUTE_TIME_ACCESS);
       	b_atime = g_file_info_get_attribute_uint64 (b_info, G_FILE_ATTRIBUTE_TIME_ACCESS);

	g_object_unref (a_file);
	g_object_unref (b_file);
	g_object_unref (a_info);
	g_object_unref (b_info);
	g_free (a_path);
	g_free (b_path);

	return a_atime > b_atime;
}

static GtkListStore *
create_store (void)
{
	GtkListStore *store;

	store = gtk_list_store_new (NUM_COLS,
	                        G_TYPE_STRING,
	                        G_TYPE_STRING,
	                        GDK_TYPE_PIXBUF);

	/* Set sort column and function */
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
	                                     sort_func,
	                                     NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
	                                  GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
	                                  GTK_SORT_ASCENDING);

	return store;
}

static gboolean
is_file_syncd (EvDC *dc, gchar *file)
{
	GList *l;
	gchar *remote_file;
	gchar *display_name;

	display_name = g_path_get_basename (file);

	for (l = dc->priv->remote_files; l; l = l->next) {
		remote_file = (gchar *) l->data;
		if (strcmp (remote_file, display_name) == 0) {
			return TRUE;
		} 
	}
	return FALSE;
}

static void
selection_changed (GtkIconView *icon_view,
	          gpointer     user_data)
{
	EvDC *dc;
	GtkTreeIter iter;
	GtkTreePath *tree_path;
	GList *list, *l;
	gchar *path = NULL;

	dc = EV_DC (user_data);
	list = gtk_icon_view_get_selected_items (icon_view);
	for (l = list; l; l = l->next) {
		tree_path = (GtkTreePath *)(l->data);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (dc->priv->store),
	                     &iter, tree_path);

		gtk_tree_model_get (GTK_TREE_MODEL (dc->priv->store), &iter,
                      COL_PATH, &path,
                      -1);
		if (dc->priv->current_file)
			g_free (dc->priv->current_file);
		dc->priv->current_file = path;
//cause we have only one data;
		if (is_file_syncd (dc, path)) {
			gtk_button_set_label (GTK_BUTTON (dc->priv->header_sync_button), _("Syncd to Baidu!"));
			gtk_widget_set_sensitive (dc->priv->header_sync_button, FALSE);
		} else {
			gtk_button_set_label (GTK_BUTTON (dc->priv->header_sync_button), _("_Send to cloud"));
			gtk_widget_set_sensitive (dc->priv->header_sync_button, TRUE);
		}
		break;
	}

	for (l = list; l; l = l->next) {
		tree_path = (GtkTreePath *) (l->data);
		gtk_tree_path_free (tree_path);
	}
	
	g_list_free (list);
}

static void
item_activated (GtkIconView *icon_view,
	          GtkTreePath *tree_path,
	          gpointer     user_data)
{
	EvDC *dc;
	GtkListStore *store;
	GtkTreeIter iter;
	gchar *path;

	dc = EV_DC (user_data);
	store = dc->priv->store;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store),
	                     &iter, tree_path);

	gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      COL_PATH, &path,
                      -1);
	printf ("path is %s\n", path);

	g_signal_emit (dc, signals[ACTIVATED], 0, path);
	g_free (path);
}

static void
ev_dc_finalize (GObject *object)
{
	EvDC *dc = EV_DC (object);

	save_tags (dc);
	if (dc->priv->current_file)
		g_free (dc->priv->current_file);

	g_list_free_full (dc->priv->monitor_dirs, g_free);
	g_list_free_full (dc->priv->pdf_files, g_free);
	g_list_free_full (dc->priv->remote_files, g_free);
	g_object_unref (dc->priv->store);
	g_object_unref (dc->priv->gf);

        if (dc->priv->child_watch_id > 0) {
                g_source_remove (dc->priv->child_watch_id);
                dc->priv->child_watch_id = 0;
        }

	if (dc->priv->tag_table)
		g_hash_table_destroy (dc->priv->tag_table);

	g_object_unref (dc->priv->gf);
	if (dc->priv->thumbail_table)
		g_hash_table_destroy (dc->priv->thumbail_table);

	G_OBJECT_CLASS (ev_dc_parent_class)->finalize (object);
}

static void
ev_dc_class_init (EvDCClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_dc_finalize;

	signals[CHANGED] =
                g_signal_new ("changed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvDCClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[ACTIVATED] =
                g_signal_new ("activated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvDCClass, activated),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EvDCPrivate));
}

static void
load_remote_files (EvDC *dc)
{
	gchar *filename;
	gchar *content = NULL;
	gchar **argv;
	gsize length;

	filename = g_strdup ("/tmp/.evince_remote_list");
	g_file_get_contents (filename, &content, &length, NULL);
	argv = g_strsplit_set (content, "\n \t", -1);

	gint i;
	for (i = 0; argv[i]; i++) {
		if (g_str_has_suffix (argv[i], ".pdf")) {
			dc->priv->remote_files = g_list_prepend (dc->priv->remote_files, g_strdup (argv[i]));
		}
	}

	g_strfreev (argv);
	g_free (content);
	g_free (filename);
}

static void
dc_job_list_child_watch (GPid            pid,
		    int                  status,
                    EvDC		*dc)
{
        printf ("upload job: child (pid:%d) done (%s:%d)",
                 (int) pid,
                 WIFEXITED (status) ? "status"
                 : WIFSIGNALED (status) ? "signal"
                 : "unknown",
                 WIFEXITED (status) ? WEXITSTATUS (status)
                 : WIFSIGNALED (status) ? WTERMSIG (status)
                 : -1);


        if (WIFEXITED (status)) {
		/*OK*/
               // int code = WEXITSTATUS (status);
		load_remote_files (dc);
        } else if (WIFSIGNALED (status)) {
		/*Fail*/ /*TODO*/
               // int num = WTERMSIG (status);
		printf ("failed to load remote files\n");
        }
		
        dc->priv->list_child_pid = -1;
}

static void
dc_job_child_watch (GPid                 pid,
		    int                  status,
                    EvDC		*dc)
{
        printf ("upload job: child (pid:%d) done (%s:%d)",
                 (int) pid,
                 WIFEXITED (status) ? "status"
                 : WIFSIGNALED (status) ? "signal"
                 : "unknown",
                 WIFEXITED (status) ? WEXITSTATUS (status)
                 : WIFSIGNALED (status) ? WTERMSIG (status)
                 : -1);


        if (WIFEXITED (status)) {
		/*OK*/
         //       int code = WEXITSTATUS (status);
        } else if (WIFSIGNALED (status)) {
		/*Fail*/ /*TODO*/
           //     int num = WTERMSIG (status);
        }
		
	gtk_button_set_label (GTK_BUTTON (dc->priv->header_sync_button), "Syncd to Baidu!");
	gtk_widget_set_sensitive (dc->priv->header_sync_button, FALSE);
	gtk_widget_set_sensitive (dc->priv->icon_view, TRUE);
        dc->priv->child_pid = -1;
}

static void
dc_load_remote_files (EvDC	*dc)
{
	pid_t child_pid;
	child_pid = fork();
	if (child_pid < 0) {
		return;
	}
        if (child_pid == 0) {
		gchar *command;
		gint status;
//Don't know if pipe could handle large buffer, use file instead 
/* TODO, if no bypy, better implement */
		command = g_strdup ("/usr/bin/bypy.py list > /tmp/.evince_remote_list");
		status = system (command);
		g_free (command);
		exit(status);
	}
		
	dc->priv->list_child_pid = child_pid;
        dc->priv->list_child_watch_id = g_child_watch_add (dc->priv->list_child_pid,
                                                      (GChildWatchFunc)dc_job_list_child_watch,
							dc);
}

static void
quick_preview_remote_file_callback (GtkButton *button,
			EvDC	*dc)
{
// TODO reload every time we resync it ? 
	static GtkWidget *menu = NULL;

	if (menu) {
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
		return;
	}
		
	GtkWidget *w;
	GList *l;

	menu = gtk_menu_new ();
      
	for (l = dc->priv->remote_files; l; l = l->next) {	
		w = gtk_menu_item_new_with_label (l->data);
      		gtk_menu_shell_append (GTK_MENU_SHELL (menu), w);
	      	gtk_widget_show (w);

      		gtk_widget_show (menu);
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, 0);
	}
}

static void
tag_item_cb (GtkMenuItem *item,
		EvDC *dc)
{
	const gchar *label;
	gint dest_tag = TAG_UNREAD;
	gint i;
	label = gtk_menu_item_get_label (item);
	for (i = 1; tag_labels[i]; i++) {
		if (strcmp (tag_labels[i], label) == 0) {
			dest_tag = i;
			break;
		}
	}
	gchar *value;
	gint cur_tag;

        value = g_hash_table_lookup (dc->priv->tag_table, dc->priv->current_file);

	if (value) {
		cur_tag = atoi (value);
	} else {
		cur_tag = TAG_UNREAD;
	}
	if (dest_tag == cur_tag) {
		return;
	} else {
		g_hash_table_replace (dc->priv->tag_table, g_strdup (dc->priv->current_file), g_strdup_printf ("%d", dest_tag));
		load_icon_view (dc);
	}
}

static void
set_tag_callback (GtkButton *button,
		  EvDC *dc)
{
	if (!dc->priv->current_file)
		return;

	static GtkWidget *tag_menu = NULL;

	if (tag_menu) {
		gtk_menu_popup (GTK_MENU (tag_menu), NULL, NULL, NULL, NULL, 0, 0);
		return;
	}
		
	GtkWidget *w;
	gint i;

	tag_menu = gtk_menu_new ();
      
	for (i = 1; tag_labels[i]; i++) {
		w = gtk_menu_item_new_with_label (tag_labels[i]);
      		gtk_menu_shell_append (GTK_MENU_SHELL (tag_menu), w);
	      	gtk_widget_show (w);

      		gtk_widget_show (tag_menu);
		gtk_menu_popup (GTK_MENU (tag_menu), NULL, NULL, NULL, NULL, 0, 0);
		g_signal_connect (w, "activate",
	                    G_CALLBACK (tag_item_cb), dc);

	}
}

static void
send_to_cloud_callback (GtkButton *button,
                        EvDC	  *dc)
{
	if (!dc->priv->current_file)
		return;

	pid_t child_pid;
	child_pid = fork();
	if (child_pid < 0) {
		return;
	}
        if (child_pid == 0) {
		gchar *command;
		gint status;

/* TODO, if no bypy, better implement */
		command = g_strdup_printf ("/usr/bin/bypy.py upload \"%s\"", dc->priv->current_file);
		status = system (command);
		g_free (command);
		exit(status);
	}
		
	gtk_button_set_label (GTK_BUTTON (dc->priv->header_sync_button), "Uploading to Baidu ...");
	gtk_widget_set_sensitive (dc->priv->header_sync_button, FALSE);
	gtk_widget_set_sensitive (dc->priv->icon_view, FALSE);

	dc->priv->child_pid = child_pid;
        dc->priv->child_watch_id = g_child_watch_add (dc->priv->child_pid,
                                                      (GChildWatchFunc)dc_job_child_watch,
						      dc);
}

enum
{
  TEXT_COL
};

static GtkWidget *
header_view_combo (EvDC *dc)
{
	GtkWidget *combo;
	GtkTreeIter iter;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkCellRenderer *renderer;

	gint i;
  	store = gtk_list_store_new (1, G_TYPE_STRING);

 	for (i = 0; tag_labels[i]; i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
                              TEXT_COL, _(tag_labels[i]),
                              -1);
        }

	model = GTK_TREE_MODEL (store);
    	combo = gtk_combo_box_new_with_model (model);
	g_object_unref (model);

	renderer = gtk_cell_renderer_text_new ();
    	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                    "text", TEXT_COL,
                                    NULL);

	return combo;
}

static void
header_view_changed_callback (GtkComboBox *box,
		EvDC *dc)
{
	dc->priv->current_header_view = gtk_combo_box_get_active (box);
	load_icon_view (dc);
}

static void
load_tags (EvDC *dc)
{
	gchar *filename = "/tmp/.evince_tag_file";
	gchar *content = NULL;
	gsize length;
	gchar **argv;
	gint i;

	g_file_get_contents (filename, &content, &length, NULL);
	argv = g_strsplit (content, "\n", -1);
	for (i = 0; argv[i]; i++) {
		gchar *path, *tag, *p;
		path = g_strdup (argv[i]);
		p = strrchr (path, ' ');
		if (!p)
			continue;
		tag = p+1;
		*p = 0;
		g_hash_table_insert (dc->priv->tag_table, path, tag);
	}

	g_strfreev (argv);
	g_free (content);
}

static void
save_tags (EvDC *dc)
{
        gchar  *filename;
        gchar  *data;
        GError *error = NULL;

	filename = "/tmp/.evince_tag_file";


	GString *tag_string = g_string_new (NULL);
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, dc->priv->tag_table);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_string_append_printf (tag_string, "%s %s", (gchar *)key, (gchar *)value);
		g_string_append_c (tag_string, '\n');
        }

	data = g_string_free (tag_string, FALSE);

        g_file_set_contents (filename, data, strlen (data), &error);
        if (error) {
                g_warning ("Failed to save print settings: %s", error->message);
                g_error_free (error);
        }
        g_free (data);
}

static void
ev_dc_init (EvDC *dc)
{
	dc->priv = G_TYPE_INSTANCE_GET_PRIVATE (dc, EV_TYPE_DC, EvDCPrivate);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (dc), GTK_ORIENTATION_VERTICAL);

	dc->priv->tag_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	dc->priv->gf = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	dc->priv->thumbail_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	dc->priv->remote_files = NULL;
	dc->priv->current_file = NULL;
//TODO: need a copy to preserve the pixmap 
	dc->priv->file_pixbuf = gdk_pixbuf_new_from_file(FILE_NAME, NULL);

	dc->priv->header_bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (dc), dc->priv->header_bar, FALSE, FALSE, 0);

	dc->priv->current_header_view = 0;
	dc->priv->header_view = header_view_combo (dc);
	gtk_combo_box_set_active (GTK_COMBO_BOX (dc->priv->header_view), dc->priv->current_header_view);
	gtk_box_pack_start (GTK_BOX (dc->priv->header_bar), dc->priv->header_view, FALSE, FALSE, 0);
	gtk_widget_show (dc->priv->header_view);
	g_signal_connect (GTK_COMBO_BOX (dc->priv->header_view), "changed", G_CALLBACK (header_view_changed_callback),
				dc);

	dc->priv->header_tag_button =  gtk_button_new_with_mnemonic (_("_Set tags"));
	gtk_box_pack_start (GTK_BOX (dc->priv->header_bar), dc->priv->header_tag_button, FALSE, FALSE, 0);
	g_signal_connect (dc->priv->header_tag_button, "clicked",
                    G_CALLBACK (set_tag_callback),
                    dc);

	dc->priv->header_sync_button =  gtk_button_new_with_mnemonic (_("_Send to cloud"));
	gtk_box_pack_start (GTK_BOX (dc->priv->header_bar), dc->priv->header_sync_button, FALSE, FALSE, 0);
	g_signal_connect (dc->priv->header_sync_button, "clicked",
                    G_CALLBACK (send_to_cloud_callback),
                    dc);
	gtk_widget_set_sensitive (dc->priv->header_sync_button, FALSE);

	dc->priv->header_share_button =  gtk_button_new_with_mnemonic (_("_Share to friend"));
	gtk_box_pack_start (GTK_BOX (dc->priv->header_bar), dc->priv->header_share_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (dc->priv->header_share_button, FALSE);

/*TODO, now only list at startup */
	dc->priv->header_remote_file_button =  gtk_button_new_with_mnemonic (_("_List remote files"));
	gtk_box_pack_start (GTK_BOX (dc->priv->header_bar), dc->priv->header_remote_file_button, FALSE, FALSE, 0);
	g_signal_connect (dc->priv->header_remote_file_button, "clicked",
                    G_CALLBACK (quick_preview_remote_file_callback),
                    dc);

	dc->priv->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dc->priv->sw),
	                                     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dc->priv->sw),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start (GTK_BOX (dc), dc->priv->sw, TRUE, TRUE, 0);

	/* Create the store and fill it with the contents of '/' */
	dc->priv->store = create_store ();

	dc->priv->icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (dc->priv->store));
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (dc->priv->icon_view), 64);
	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (dc->priv->icon_view),
					  GTK_SELECTION_SINGLE);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (dc->priv->icon_view), COL_DISPLAY_NAME);
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (dc->priv->icon_view), COL_PIXBUF);

	/* Connect to the "item-activated" signal */
	g_signal_connect (dc->priv->icon_view, "item-activated",
	                  G_CALLBACK (item_activated), dc);
	g_signal_connect (dc->priv->icon_view, "selection_changed",
	                  G_CALLBACK (selection_changed), dc);
	gtk_container_add (GTK_CONTAINER (dc->priv->sw), dc->priv->icon_view);

	gtk_widget_grab_focus (dc->priv->icon_view);

	dc->priv->child_watch_id = 0;
	dc_load_remote_files (dc);

	load_tags (dc);
	load_pdf_dirs (dc);
	load_pdf_files (dc);
	load_icon_view (dc);
}

EvDC *
ev_dc_new ()
{
        EvDC *dc;

        dc = EV_DC (g_object_new (EV_TYPE_DC, NULL));

        return dc;
}
