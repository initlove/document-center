#include "ev-dc.h"
/* Icon View/Icon View Basics
 *
 * The GtkIconView widget is used to display and manipulate icons.
 * It uses a GtkTreeModel for data storage, so the list store
 * example might be helpful.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#endif


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

#include "ev-dc.h"

enum {
	CHANGED,
        ACTIVATE,

	N_SIGNALS
};

static guint signals[N_SIGNALS] = {0, };

struct _EvDCPrivate {
	GnomeDesktopThumbnailFactory *gf;
	GList		*monitor_dirs;
	GList           *pdf_files;

	GdkPixbuf 	*file_pixbuf;
	GtkWidget	*header_bar;
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

static void
fill_store (EvDC *dc)
{
	GtkListStore *store;
	gchar *display_name;
	const gchar *path;
	GList *pdf_files, *l;
	GtkTreeIter iter;
	GnomeDesktopThumbnailFactory *gf;
	
	store = dc->priv->store;

	pdf_files = dc->priv->pdf_files;
	/* First clear the store */
	gtk_list_store_clear (store);

	gf = dc->priv->gf;

	for (l = pdf_files; l; l = l->next) {
		path = (gchar *) l->data;
		gchar *thumbail_path;
//TODO: need to unref 
		GdkPixbuf *thumbail_pixbuf;
//TODO: need g_free?
		thumbail_path = gnome_desktop_thumbnail_path_for_uri (path, GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
printf ("thumbail path %s\n", thumbail_path);
		thumbail_pixbuf = gdk_pixbuf_new_from_file(thumbail_path, NULL);
		if (!thumbail_pixbuf) {
			thumbail_pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (gf, path, "application/pdf");
			gnome_desktop_thumbnail_factory_save_thumbnail (gf, thumbail_pixbuf, path, 0);
		}
		display_name = g_path_get_basename (path);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
	                        COL_PATH, path,
	                        COL_DISPLAY_NAME, display_name,
	                        COL_PIXBUF, thumbail_pixbuf ? thumbail_pixbuf : dc->priv->file_pixbuf,
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
	gchar *name_a, *name_b;
	int ret = TRUE;

	/* TODO */

	gtk_tree_model_get (model, a,
	                COL_DISPLAY_NAME, &name_a,
	                -1);

	gtk_tree_model_get (model, b,
	                COL_DISPLAY_NAME, &name_b,
	                -1);


	g_free (name_a);
	g_free (name_b);

	return ret;
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

static void
item_activated (GtkIconView *icon_view,
	          GtkTreePath *tree_path,
	          gpointer     user_data)
{
	GtkListStore *store;
	GtkTreeIter iter;
	store = GTK_LIST_STORE (user_data);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store),
	                     &iter, tree_path);
}
static void
ev_dc_finalize (GObject *object)
{
	EvDC *dc = EV_DC (object);

	g_list_free_full (dc->priv->monitor_dirs, g_free);
	g_list_free_full (dc->priv->pdf_files, g_free);
	g_object_unref (dc->priv->store);
	g_object_unref (dc->priv->gf);

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
#if 0
        signals[ACTIVATE] =
                g_signal_new ("activate",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvDCClass, activate_link),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              G_TYPE_OBJECT);
#endif
	g_type_class_add_private (object_class, sizeof (EvDCPrivate));
}

static void
ev_dc_init (EvDC *dc)
{
	dc->priv = G_TYPE_INSTANCE_GET_PRIVATE (dc, EV_TYPE_DC, EvDCPrivate);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (dc), GTK_ORIENTATION_VERTICAL);

//TODO: need a copy to preserve the pixmap 
	dc->priv->file_pixbuf = gdk_pixbuf_new_from_file(FILE_NAME, NULL);

	dc->priv->header_bar = gtk_label_new (_("PDF Center"));
	gtk_box_pack_start (GTK_BOX (dc), dc->priv->header_bar, FALSE, FALSE, 0);

	dc->priv->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dc->priv->sw),
	                                     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dc->priv->sw),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start (GTK_BOX (dc), dc->priv->sw, TRUE, TRUE, 0);

	dc->priv->gf = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	/* Create the store and fill it with the contents of '/' */
	dc->priv->store = create_store ();
	load_pdf_dirs (dc);
	load_pdf_files (dc);
	fill_store (dc);

	dc->priv->icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (dc->priv->store));
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (dc->priv->icon_view), 64);
	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (dc->priv->icon_view),
					  GTK_SELECTION_SINGLE);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (dc->priv->icon_view), COL_DISPLAY_NAME);
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (dc->priv->icon_view), COL_PIXBUF);

	/* Connect to the "item-activated" signal */
	g_signal_connect (dc->priv->icon_view, "item-activated",
	                  G_CALLBACK (item_activated), dc->priv->store);
	gtk_container_add (GTK_CONTAINER (dc->priv->sw), dc->priv->icon_view);

	gtk_widget_grab_focus (dc->priv->icon_view);

}

EvDC *
ev_dc_new ()
{
        EvDC *dc;

        dc = EV_DC (g_object_new (EV_TYPE_DC, NULL));

        return dc;
}
