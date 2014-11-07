/* gcc youtube-downloader-ui.c -o youtube-downloader-ui `pkg-config --cflags --libs gtk+-3.0` */
#include <gtk/gtk.h>
#include <stdlib.h>

#define COMMAND "/usr/bin/youtube-dl"
#define REGEX "^\\[download\\] +([0-9]{1,3}\\.[0-9])% of ([0-9]+\\.[0-9]+)([Mk]) " \
              "at +([0-9]+\\.[0-9]{2})([Mk]/s) ETA 0?([0-9]{1,2}):0?([0-9]{1,2}).*$"

#define pluralize(string, singular, plural) ((string[0] == '1' && string[1] == '\0') ? singular : plural)

typedef struct
{
	GtkWidget *box;
	GtkWidget *entry;
	GtkWidget *progress;
	GtkWidget *label;
} UI;

gboolean updating = FALSE;

static void parse_output (gchar *string, UI *ui);
static void child_watch (GPid id, gint status, UI *ui);
static gboolean output_watch (GIOChannel *channel, GIOCondition condition, UI *ui);
static gboolean error_watch (GIOChannel *channel, GIOCondition condition, UI *ui);
static gboolean update (UI *ui);
static void validate (GtkEditable *editable, GtkButton *button);
static void execute (GtkButton *button, UI *ui);

int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *content;
	GtkWidget *label;
	GtkWidget *button;
	GError *error;
	UI *ui;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	content = gtk_vbox_new (FALSE, 6);
	label = gtk_label_new ("Dirección del vídeo:");
	button = gtk_button_new_with_mnemonic ("_Descargar");

	ui = g_new0 (UI, 1);
	ui->box = gtk_hbox_new (FALSE, 6);
	ui->entry = gtk_entry_new ();
	ui->progress = gtk_progress_bar_new ();
	ui->label = gtk_label_new ("");

	error = NULL;

	gtk_window_set_title (GTK_WINDOW (window), "Youtube downloader");
	gtk_window_set_icon_from_file (GTK_WINDOW (window), "icon.png", &error);
	gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (ui->progress), TRUE);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (ui->progress), " ");
	gtk_entry_set_width_chars (GTK_ENTRY (ui->entry), 42);
	gtk_entry_set_max_length (GTK_ENTRY (ui->entry), 50);
	gtk_misc_set_alignment (GTK_MISC (ui->label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (ui->label), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	if (error)
	{
		g_warning ("Error: %s", error->message);
		g_error_free (error);
	}

	gtk_container_add (GTK_CONTAINER (window), content);
	gtk_container_add (GTK_CONTAINER (content), ui->box);
	gtk_container_add (GTK_CONTAINER (ui->box), label);
	gtk_container_add (GTK_CONTAINER (ui->box), ui->entry);
	gtk_container_add (GTK_CONTAINER (ui->box), button);
	gtk_container_add (GTK_CONTAINER (content), ui->progress);
	gtk_container_add (GTK_CONTAINER (content), ui->label);

	gtk_widget_show_all (window);

	g_signal_connect (G_OBJECT (ui->entry), "changed", G_CALLBACK (validate), button);
	g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (execute), ui);
	g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);

	validate (GTK_EDITABLE (ui->entry), GTK_BUTTON (button));

	gtk_main ();

	return (EXIT_SUCCESS);
}

#ifdef G_OS_WIN32
int APIENTRY
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  return main ((int) nCmdShow, (char **) lpCmdLine);
}
#endif

static void
validate (GtkEditable *editable, GtkButton *button)
{
	gchar *value;
	value = gtk_editable_get_chars (editable, 0, -1);
	gtk_widget_set_sensitive (GTK_WIDGET (button), value[0] != '\0');
	g_free (value);
}

static void
parse_output (gchar *string, UI *ui)
{
	GError *error = NULL;
	GRegex *regex;
	GMatchInfo *matches;

	regex = g_regex_new (REGEX, 0, 0, &error);

	if (error)
	{
		g_error ("%s", error->message);
		g_error_free (error);
	}
	else
	{
		gboolean result;

		result = g_regex_match (regex, string, 0, &matches); /* XXX */

		if (result)
		{
			updating = FALSE;

			const gchar *link = gtk_entry_get_text (GTK_ENTRY (ui->entry));

			gchar *buffer;
			gchar *caption;
			gchar *percent;
			gchar *size;
			gchar *size_unit;
			gchar *speed;
			gchar *speed_unit;
			gchar *minutes;
			gchar *seconds;
			gdouble downloaded;
			gdouble fraction;

			percent = g_match_info_fetch (matches, 1);
			size = g_match_info_fetch (matches, 2);
			size_unit = g_match_info_fetch (matches, 3);
			speed = g_match_info_fetch (matches, 4);
			speed_unit = g_match_info_fetch (matches, 5);
			minutes = g_match_info_fetch (matches, 6);
			seconds = g_match_info_fetch (matches, 7);

			caption = g_strdup_printf ("Descargando <b>%s</b>.\n%s alrededor de <b>%s</b> %s y <b>%s</b> %s a <b>%s%s</b>.",
			                           link, pluralize (minutes, "Falta", "Faltan"), minutes,
			                           pluralize (minutes, "minuto", "minutos"), seconds,
			                           pluralize (seconds, "segundo", "segundos"), speed, speed_unit);

			fraction = g_strtod (percent, NULL) / 100.0;
			downloaded = g_strtod (size, NULL) * fraction;
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (ui->progress), fraction);
			buffer = g_strdup_printf ("Descargados %.02g%s de %s%s (%s%%)", downloaded, size_unit, size, size_unit, percent);
			gtk_progress_bar_set_text (GTK_PROGRESS_BAR (ui->progress), buffer);

			gtk_label_set_markup (GTK_LABEL (ui->label), caption);

			g_free (caption);
			g_free (buffer);
			g_free (percent);
			g_free (size);
			g_free (size_unit);
			g_free (speed);
			g_free (speed_unit);
			g_free (minutes);
			g_free (seconds);

			while (gtk_events_pending ())
			{
				gtk_main_iteration ();
			}
		}

		g_match_info_matches (matches);
	}
 
	g_regex_unref (regex);
}

static void
child_watch (GPid id, gint status, UI *ui)
{
	g_spawn_close_pid (id);
	gtk_widget_set_sensitive (ui->box, TRUE);
	updating = FALSE;
}

static gboolean
output_watch (GIOChannel *channel, GIOCondition condition, UI *ui)
{
	gchar *string;
	gsize size;

	if (condition == G_IO_HUP)
	{
		g_io_channel_unref (channel);

		gtk_label_set_text (GTK_LABEL (ui->label), "Descarga completa.");

		//g_debug ("output condition == G_IO_HUP");

		return (FALSE);
	}

	g_io_channel_read_line (channel, &string, &size, NULL, NULL);

	parse_output (string, ui);

	//g_debug ("%s", string);

	g_free (string);

	return (TRUE);
}

static gboolean
error_watch (GIOChannel *channel, GIOCondition condition, UI *ui)
{
	gchar *string;
	gsize size;

	if (condition == G_IO_HUP)
	{
		g_io_channel_unref (channel);

		gtk_label_set_markup (GTK_LABEL (ui->label), "<span color='red' weight='heavy'>Error inesperado.</span>");

		//g_debug ("error condition == G_IO_HUP");

		return (FALSE);
	}

	g_io_channel_read_line (channel, &string, &size, NULL, NULL);

	g_debug ("%s", string);

	g_free (string);

	return (TRUE);
}

static gboolean
update (UI *ui)
{
	if (updating)
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR (ui->progress));
	else
	{
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (ui->progress), " ");
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (ui->progress), 0.0);
	}

	return (updating);
}

static void
execute (GtkButton *button, UI *ui)
{
	const gchar *link = gtk_entry_get_text (GTK_ENTRY (ui->entry));

	updating = TRUE;

	gtk_widget_set_sensitive (ui->box, FALSE);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (ui->progress), "Consultando al servidor");
	gtk_label_set_markup (GTK_LABEL (ui->label), "<i>Obteniendo información del video.</i>");
	g_timeout_add (250, (GSourceFunc) update, ui);

	gchar *command[] = { COMMAND, (gchar *) link, NULL };

	GIOChannel *output_channel;
	GIOChannel *error_channel;
	GPid id;
	gint fd_output;
	gint fd_error;
	GError *error;
	gboolean result;

	error = NULL;

	result = g_spawn_async_with_pipes (NULL, command, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
	                                   NULL, &id, NULL, &fd_output, &fd_error, &error);

	if (!result)
	{
		if (error)
		{
			gchar *message;
			message = g_strdup_printf ("<span color='red' weight='heavy'>%s</span>", error->message);
			gtk_label_set_markup (GTK_LABEL (ui->label), message);
			g_error_free (error);
			g_free (message);
		}
		else
			gtk_label_set_text (GTK_LABEL (ui->label), "g_spawn_async_with_pipes failed");

		return;
	}

	g_child_watch_add (id, (GChildWatchFunc) child_watch, ui);

#ifdef G_OS_WIN32
	output_channel = g_io_channel_win32_new_fd (fd_output);
	error_channel = g_io_channel_win32_new_fd (fd_error);
#else
	output_channel = g_io_channel_unix_new (fd_output);
	error_channel = g_io_channel_unix_new (fd_error);
#endif

	g_io_add_watch (output_channel, G_IO_IN | G_IO_HUP, (GIOFunc) output_watch, ui);
	g_io_add_watch (error_channel, G_IO_IN | G_IO_HUP, (GIOFunc) error_watch, ui);
}
