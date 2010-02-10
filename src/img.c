#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "finger.h"

#include <gtk/gtk.h>

/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;
static GtkWidget *window;

static void draw_point( GtkWidget *widget, gdouble x, gdouble y, long color);

/**
 * Main draw algorithm ;)
 */
static int draw(){
	int imgsize = (int) sizeof(finger) - 1;

	int c = 0;
	int i = 3;
	int z = 3;
	int y = 0;

	for (i; i < imgsize - 1; i++){
		if ((finger[i] == 0x01) && (finger[i+1] == 0xFE)){
			i+=6;
			c = 0;
			y++;
		} else if (c <= 200){
			draw_point(window, c, y, finger[i]);
			c++;
		}
	}
}

/* Create a new backing pixmap of the appropriate size */
static gint configure_event( GtkWidget         *widget,
                             GdkEventConfigure *event )
{
  if (pixmap)
    gdk_pixmap_unref(pixmap);

  pixmap = gdk_pixmap_new(widget->window,
                          widget->allocation.width,
                          widget->allocation.height,
                          -1);
  gdk_draw_rectangle (pixmap,
                      widget->style->white_gc,
                      TRUE,
                      0, 0,
                      widget->allocation.width,
                      widget->allocation.height);
draw();

  return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint expose_event( GtkWidget      *widget,
                          GdkEventExpose *event )
{
  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);
  return FALSE;
}

static void draw_point( GtkWidget *widget, gdouble x, gdouble y, long color) {
	GdkColor colors[1];
	GdkGC *color_gc = gdk_gc_new (widget->window);
	if (pixmap != NULL) {
		GdkRectangle update_rect;
		update_rect.x = x;
		update_rect.y = y;
		update_rect.width = 1;
		update_rect.height = 1;
		colors[0].red = (color>>16&0xff)*256;
		colors[0].green = (color>>8&0xff)*256;
		colors[0].blue = (color>>0&0xff)*256;
		gdk_colormap_alloc_color (gdk_colormap_get_system(), &colors[0], FALSE, TRUE);
		gdk_gc_set_foreground (color_gc, colors);
		gdk_draw_point(pixmap, color_gc, x, y);
	} else {
		printf("draw_point: Error, pixmap1 not defined\n");
	}
}

void quit ()
{
  gtk_exit (0);
}

int main( int   argc, 
          char *argv[] )
{
  GtkWidget *drawing_area;
  GtkWidget *vbox;
  GtkWidget *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (window, "Test Input");

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  gtk_signal_connect (GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (quit), NULL);

  /* Create the drawing area */

  drawing_area = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 200, 1000);
  gtk_box_pack_start (GTK_BOX (vbox), drawing_area, TRUE, TRUE, 0);

  gtk_widget_show (drawing_area);

  /* Signals used to handle backing pixmap */

  gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
                      (GtkSignalFunc) expose_event, NULL);
  gtk_signal_connect (GTK_OBJECT(drawing_area),"configure_event",
                      (GtkSignalFunc) configure_event, NULL);

  gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
                         | GDK_LEAVE_NOTIFY_MASK
                         | GDK_BUTTON_PRESS_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_POINTER_MOTION_HINT_MASK);

  /* .. And a quit button */
  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (window));
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
