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

#define FILE_NAME "/usr/share/icons/hicolor/32x32/apps/flash-player-properties.png"
#define FOLDER_NAME "/usr/share/icons/gnome/32x32/places/gtk-directory.png"

enum
{
	COL_PATH,
	COL_DISPLAY_NAME,
	COL_PIXBUF,
	COL_IS_DIRECTORY,
	NUM_COLS
};

static GdkPixbuf *file_pixbuf = NULL;
static GdkPixbuf *folder_pixbuf = NULL;

/* Loads the images for the demo and returns whether the operation succeeded */
static void
load_pixbufs (void)
{
	if (file_pixbuf)
	  return; /* already loaded earlier */

	file_pixbuf = gdk_pixbuf_new_from_file(FILE_NAME, NULL);
	/* resources must load successfully */
	g_assert (file_pixbuf);

	folder_pixbuf = gdk_pixbuf_new_from_file (FOLDER_NAME, NULL);
	g_assert (folder_pixbuf);
}

static GList *
load_pdf_dirs ()
{
	GList *l = NULL;

	l = g_list_append (l, g_strdup (g_get_home_dir ()));

	return l;
}

static GList *
load_pdfs (gchar *parent, GList *pdf_files)
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
				pdf_files = load_pdfs (path, pdf_files);
			} else if (g_str_has_suffix (path, ".pdf")) {
				pdf_files = g_list_prepend (pdf_files, g_strdup (path));
			}
	    		g_free (path);
		}
	}
	g_dir_close (dir);

	return pdf_files;
}

static GList *
load_pdf_files ()
{
	GList *pdf_files = NULL;
	GList *pdf_paths = NULL;
	GList *l;

	pdf_paths = load_pdf_dirs ();
	for (l = pdf_paths; l; l = l->next) {
		pdf_files = load_pdfs (l->data, pdf_files);
	}

	g_list_free_full (pdf_paths, g_free);

	return pdf_files;
}

static void
fill_store (GtkListStore *store)
{
	gchar *display_name;
	const gchar *path;
	GList *pdf_files, *l;
	GtkTreeIter iter;
	gchar *is_dir = FALSE;
	
	pdf_files = load_pdf_files ();
	/* First clear the store */
	gtk_list_store_clear (store);

	GnomeDesktopThumbnailFactory *gf;
	gf = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

	for (l = pdf_files; l; l = l->next) {
		path = (gchar *) l->data;
		gchar *thumbail_path;
//TODO: need to unref 
		GdkPixbuf *thumbail_pixbuf;
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
	                        COL_IS_DIRECTORY, is_dir,
	                        COL_PIXBUF, thumbail_pixbuf ? thumbail_pixbuf : file_pixbuf,
	                        -1);
		g_free (display_name);
	}
	g_list_free_full (pdf_files, g_free);
}

static gint
sort_func (GtkTreeModel *model,
	     GtkTreeIter  *a,
	     GtkTreeIter  *b,
	     gpointer      user_data)
{
	gboolean is_dir_a, is_dir_b;
	gchar *name_a, *name_b;
	int ret;

	/* We need this function because we want to sort
	 * folders before files.
	 */


	gtk_tree_model_get (model, a,
	                COL_IS_DIRECTORY, &is_dir_a,
	                COL_DISPLAY_NAME, &name_a,
	                -1);

	gtk_tree_model_get (model, b,
	                COL_IS_DIRECTORY, &is_dir_b,
	                COL_DISPLAY_NAME, &name_b,
	                -1);

	if (!is_dir_a && is_dir_b)
	  ret = 1;
	else if (is_dir_a && !is_dir_b)
	  ret = -1;
	else
	  {
	ret = g_utf8_collate (name_a, name_b);
	  }

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
	                        GDK_TYPE_PIXBUF,
	                        G_TYPE_BOOLEAN);

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

GtkWidget *
ev_dc_new ()
{
	GtkWidget *sw;
	GtkWidget *icon_view;
	GtkListStore *store;
	GtkWidget *dc;
	GtkWidget *label_bar;

	load_pixbufs ();

	dc = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	label_bar = gtk_label_new (_("PDF Center"));
	gtk_box_pack_start (GTK_BOX (dc), label_bar, FALSE, FALSE, 0);


	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
	                                     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);

	gtk_box_pack_start (GTK_BOX (dc), sw, TRUE, TRUE, 0);

	/* Create the store and fill it with the contents of '/' */
	store = create_store ();
	fill_store (store);

	icon_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_icon_view_set_item_width (icon_view, 64);
	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (icon_view),
	                                  GTK_SELECTION_MULTIPLE);
	g_object_unref (store);

	/* We now set which model columns that correspond to the text
	 * and pixbuf of each item
	 */
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_view), COL_DISPLAY_NAME);
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_view), COL_PIXBUF);

	/* Connect to the "item-activated" signal */
	g_signal_connect (icon_view, "item-activated",
	                  G_CALLBACK (item_activated), store);
	gtk_container_add (GTK_CONTAINER (sw), icon_view);

	gtk_widget_grab_focus (icon_view);

	return dc;
}
