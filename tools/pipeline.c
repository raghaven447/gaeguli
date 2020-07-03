/**
 *  Copyright 2019 SK Telecom, Co., Ltd.
 *    Author: Jeongseok Kim <jeongseok.kim@sk.com>
 *
 */

#include "config.h"

#include "pipeline.h"

#include <glib-unix.h>
#include <gio/gio.h>
#include <gst/gst.h>

static guint signal_watch_intr_id;
static guint target_id;

static void
activate (GApplication * app, gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  const gchar *srt_uri = NULL;

  GaeguliPipeline *pipeline = user_data;

  g_application_hold (app);

  srt_uri = g_object_get_data (G_OBJECT (app), "srt_uri");
  g_print ("Streaming to %s\n", srt_uri);
  target_id = gaeguli_pipeline_add_srt_target (pipeline, srt_uri, &error);
}

static gboolean
intr_handler (gpointer user_data)
{
  GaeguliPipeline *pipeline = user_data;
  g_autoptr (GError) error = NULL;

  gaeguli_pipeline_remove_target (pipeline, target_id, &error);

  g_debug ("target removed");

  return G_SOURCE_REMOVE;
}

static void
stream_stopped_cb (GaeguliPipeline * pipeline, guint target_id,
    gpointer user_data)
{
  GApplication *app = user_data;

  g_debug ("stream stopped");
  g_application_release (app);
}

int
main (int argc, char *argv[])
{
  gboolean help = FALSE;
  const gchar *device = DEFAULT_VIDEO_SOURCE_DEVICE;
  const gchar **uri = NULL;
  gboolean overlay = FALSE;
  int result;

  g_autoptr (GError) error = NULL;
  g_autoptr (GApplication) app = g_application_new (NULL, 0);

  g_autoptr (GOptionContext) context = NULL;
  GOptionEntry entries[] = {
    {"device", 'd', 0, G_OPTION_ARG_FILENAME, &device, NULL, NULL},
    {"clock-overlay", 'c', 0, G_OPTION_ARG_NONE, &overlay, NULL, NULL},
    {"help", '?', 0, G_OPTION_ARG_NONE, &help, NULL, NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uri, NULL, NULL},
    {NULL}
  };

  g_autoptr (GaeguliPipeline) pipeline = NULL;

  gst_init (&argc, &argv);

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("%s\n", error->message);
    return -1;
  }

  if (help) {
    g_autofree gchar *text = g_option_context_get_help (context, FALSE, NULL);
    g_printerr ("%s\n", text);
    return -1;
  }

  if (!uri) {
    g_printerr ("SRT uri not specified\n");
    return -1;
  }
  if (!g_str_has_prefix (uri[0], "srt://")) {
    g_printerr ("Invalid SRT uri %s\n", uri[0]);
    return -1;
  }

  pipeline = gaeguli_pipeline_new_full (DEFAULT_VIDEO_SOURCE, device,
      DEFAULT_ENCODING_METHOD);
  g_object_set (pipeline, "clock-overlay", overlay, NULL);

  signal_watch_intr_id =
      g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, pipeline);

  g_signal_connect (pipeline, "stream-stopped", G_CALLBACK (stream_stopped_cb),
      app);

  g_signal_connect (app, "activate", G_CALLBACK (activate), pipeline);
  g_object_set_data_full (G_OBJECT (app), "srt_uri", g_strdup (uri[0]), g_free);

  result = g_application_run (app, argc, argv);

  gaeguli_pipeline_stop (pipeline);

  if (signal_watch_intr_id > 0)
    g_source_remove (signal_watch_intr_id);

  return result;
}
