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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA..
 *
 */
#ifndef _EV_DC_H_
#define _EV_DC_H_

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define EV_TYPE_DC            (ev_dc_get_type ())
#define EV_DC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_DC, EvDC))
#define EV_DC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_DC, EvDCClass))
#define EV_IS_DC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_DC))
#define EV_IS_DC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_DC))
#define EV_DC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_DC, EvDCClass))

typedef struct _EvDC		EvDC;
typedef struct _EvDCPrivate	EvDCPrivate;
typedef struct _EvDCClass		EvDCClass;

struct _EvDC
{
        GtkBox base_instance;

	/*< private >*/
	EvDCPrivate *priv;
};

struct _EvDCClass
{
	GtkBoxClass base_class;

	void (* changed)        (EvDC *dc);
        void (* activated) 	(EvDC *dc,
				const gchar *path);
};

GType      ev_dc_get_type         (void);
EvDC	  *ev_dc_new 		  (void);

G_END_DECLS

#endif
