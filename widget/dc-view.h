/*
   Copyright 2014, SUSE, Inc.

   The Gnome appwidget lib is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome appwidget lib is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: David Liang <dliang@suse.com>
*/

#ifndef __DC_VIEW_H__
#define __DC_VIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DC_TYPE_VIEW            (dc_view_get_type ())
#define DC_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DC_TYPE_VIEW, DCView))
#define DC_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  DC_TYPE_VIEW, DCViewClass))
#define DC_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DC_TYPE_VIEW))
#define DC_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  DC_TYPE_VIEW))
#define DC_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  DC_TYPE_VIEW, DCViewClass))

typedef struct _DCView DCView;
typedef struct _DCViewClass DCViewClass;


GType		dc_view_get_type	(void);
DCView *	dc_view_new		(void);

G_END_DECLS

#endif
