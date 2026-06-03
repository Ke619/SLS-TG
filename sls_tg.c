#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <gst/gst.h>

#define CONFIG_DIR  "/.config/sls-tg"
#define MUSIC_FILE  "/.config/sls-tg/music"

typedef struct {
    GtkWidget *window;
    GtkWidget *btn;
    GtkWidget *close_btn;
    GtkWidget *logo_image;
    GtkWidget *log_view;
    GtkTextBuffer *log_buf;
    GtkWidget *status_label;
    GtkWidget *entry_username;
    GtkWidget *entry_password;
    GtkWidget *entry_appid;
    GtkCssProvider *css_provider;
    GtkWidget *outer_frame;
    GtkWidget *overlay;
    GtkWidget *dim_layer;
    GtkWidget *footer_link;
    GstElement *music_player;
    GstElement *sfx_click;
    GstElement *sfx_hover;
    int music_playing;
    guint hold_timer;
    char bin_path[512];
    char icon_path[512];
} AppWidgets;

static const char *CSS =
    "window { background-color: #cc2200; }"
    "image { background-color: #000000; }"
    "#logo_box { background-color: transparent; }"
    "#outer_frame { background-color: #000000; margin: 3px; }"
    "#title { color: #cc2200; font-size: 22px; font-weight: bold; letter-spacing: 4px; }"
    "#subtitle { color: #aaaaaa; font-size: 10px; letter-spacing: 5px; }"
    "#run_btn { background: #0d0000; color: #cc2200; border: 2px solid #cc2200;"
    "  font-size: 15px; font-weight: bold; letter-spacing: 3px; padding: 10px 40px; border-radius: 0; }"
    "#run_btn:hover { background-color: #1a0000; color: #ff3300; }"
    "#run_btn:active { background-color: #330000; color: #ff3300; }"
    "#run_btn:disabled { background-color: #0d0d0d; color: #333; border-color: #333; }"
    "#close_btn { background: transparent; color: #cc2200; border: none;"
    "  font-size: 18px; font-weight: bold; padding: 0 8px; min-width: 0; min-height: 0; }"
    "#close_btn:hover { color: #ff3300; }"
    "#close_btn:active { color: #880000; }"
    "#topbar { background-color: #000000; }"
    "#header { background-color: #000000; }"
    "#status { color: #cc2200; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#status_done { color: #228822; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#status_error { color: #ff3300; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#log { background-color: #000000; color: #aaaaaa; font-family: monospace; font-size: 12px; }"
    "#log text { background-color: #000000; color: #aaaaaa; }"
    "scrolledwindow { }"
    "#field_label { color: #aaaaaa; font-size: 11px; letter-spacing: 2px; }"
    "entry { background-color: #0d0000; color: #cc2200; border: 1px solid #cc2200;"
    "  border-radius: 0; padding: 6px 10px; font-size: 13px; }"
    "entry:focus { border-color: #ff3300; }"
    "#sep { background-color: #2a0000; min-width: 1px; }"
    "#footer { color: #aaaaaa; font-size: 10px; }"
    "#dim_layer { background-color: rgba(0,0,0,0.75); }";

static void save_music(int playing) {
    const char *home = g_get_home_dir();
    char dir_path[512], file_path[512];
    snprintf(dir_path,  sizeof(dir_path),  "%s%s", home, CONFIG_DIR);
    snprintf(file_path, sizeof(file_path), "%s%s", home, MUSIC_FILE);
    mkdir(dir_path, 0755);
    FILE *f = fopen(file_path, "w");
    if (f) { fprintf(f, "%d", playing); fclose(f); }
}

static int load_music() {
    const char *home = g_get_home_dir();
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", home, MUSIC_FILE);
    FILE *f = fopen(file_path, "r");
    if (!f) return 0;
    int playing = 0;
    fscanf(f, "%d", &playing);
    fclose(f);
    return (playing == 1) ? 1 : 0;
}

static gboolean on_music_bus(GstBus *bus, GstMessage *msg, gpointer data) {
    GstElement *player = (GstElement *)data;
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        gst_element_seek_simple(player, GST_FORMAT_TIME,
            GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
    }
    return TRUE;
}

static void play_sfx(GstElement *sfx) {
    if (!sfx) return;
    gst_element_seek_simple(sfx, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
    gst_element_set_state(sfx, GST_STATE_PLAYING);
}

static gboolean on_btn_enter(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    play_sfx(w->sfx_hover);
    return FALSE;
}

static void on_btn_click(GtkWidget *widget, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    play_sfx(w->sfx_click);
}

static void start_music(AppWidgets *w) {
    if (!w->music_player) return;
    GstBus *bus = gst_element_get_bus(w->music_player);
    gst_bus_add_watch(bus, on_music_bus, w->music_player);
    gst_object_unref(bus);
    gst_element_set_state(w->music_player, GST_STATE_PLAYING);
    w->music_playing = 1;
    save_music(1);
}

static void stop_music(AppWidgets *w) {
    if (!w->music_player) return;
    gst_element_set_state(w->music_player, GST_STATE_NULL);
    w->music_playing = 0;
    save_music(0);
}

static void toggle_music(AppWidgets *w) {
    if (w->music_playing) stop_music(w);
    else start_music(w);
}

static gboolean on_hold_timeout(gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    w->hold_timer = 0;
    toggle_music(w);
    return G_SOURCE_REMOVE;
}

static gboolean on_logo_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    if (event->button == 1) {
        if (w->hold_timer) g_source_remove(w->hold_timer);
        w->hold_timer = g_timeout_add(3000, on_hold_timeout, w);
    }
    return FALSE;
}

static gboolean on_logo_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    if (event->button == 1 && w->hold_timer) {
        g_source_remove(w->hold_timer);
        w->hold_timer = 0;
    }
    return FALSE;
}

static gboolean append_log(gpointer data) {
    char **args = (char **)data;
    AppWidgets *w = (AppWidgets *)args[0];
    const char *text = args[1];
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(w->log_buf, &end);
    gtk_text_buffer_insert(w->log_buf, &end, text, -1);
    gtk_text_buffer_insert(w->log_buf, &end, "\n", -1);
    GtkScrolledWindow *scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(w->log_view));
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(scroll);
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
    free(args[1]);
    free(args);
    return G_SOURCE_REMOVE;
}

static gboolean on_done(gpointer data) {
    char **args = (char **)data;
    AppWidgets *w = (AppWidgets *)args[0];
    int code = atoi(args[1]);
    gtk_widget_set_sensitive(w->btn, TRUE);
    if (code == 0) {
        gtk_label_set_text(GTK_LABEL(w->status_label), "✓ DONE");
        gtk_widget_set_name(w->status_label, "status_done");
    } else {
        gtk_label_set_text(GTK_LABEL(w->status_label), "✗ ERROR");
        gtk_widget_set_name(w->status_label, "status_error");
    }
    free(args[1]);
    free(args);
    return G_SOURCE_REMOVE;
}

static void log_from_thread(AppWidgets *w, const char *text) {
    char **args = malloc(sizeof(char *) * 2);
    args[0] = (char *)w;
    args[1] = strdup(text);
    g_idle_add(append_log, args);
}

static void done_from_thread(AppWidgets *w, int code) {
    char **args = malloc(sizeof(char *) * 2);
    args[0] = (char *)w;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", code);
    args[1] = strdup(buf);
    g_idle_add(on_done, args);
}

typedef struct {
    AppWidgets *w;
    char cmd[2048];
} ThreadData;

static gpointer run_thread(gpointer data) {
    ThreadData *td = (ThreadData *)data;
    AppWidgets *w = td->w;
    log_from_thread(w, "[ LAUNCHING TICKET-GRABBER... ]");
    FILE *fp = popen(td->cmd, "r");
    free(td);
    if (!fp) {
        log_from_thread(w, "[ ERROR: failed to launch ]");
        done_from_thread(w, 1);
        return NULL;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        log_from_thread(w, line);
    }
    int ret = pclose(fp);
    done_from_thread(w, WEXITSTATUS(ret));
    return NULL;
}

static void on_run_clicked(GtkWidget *btn, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(w->entry_username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(w->entry_password));
    const char *appid    = gtk_entry_get_text(GTK_ENTRY(w->entry_appid));
    if (strlen(username) == 0 || strlen(password) == 0 || strlen(appid) == 0) {
        gtk_label_set_text(GTK_LABEL(w->status_label), "⚠ FILL ALL FIELDS");
        gtk_widget_set_name(w->status_label, "status_error");
        return;
    }
    gtk_widget_set_sensitive(w->btn, FALSE);
    gtk_label_set_text(GTK_LABEL(w->status_label), "RUNNING...");
    gtk_widget_set_name(w->status_label, "status");
    gtk_text_buffer_set_text(w->log_buf, "", -1);
    ThreadData *td = malloc(sizeof(ThreadData));
    td->w = w;
    snprintf(td->cmd, sizeof(td->cmd), "%s '%s' '%s' '%s' 2>&1",
             w->bin_path, username, password, appid);
    GThread *thread = g_thread_new("runner", run_thread, td);
    g_thread_unref(thread);
}

static gboolean on_topbar_drag(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    if (event->button == 1)
        gtk_window_begin_move_drag(GTK_WINDOW(w->window),
            event->button, event->x_root, event->y_root, event->time);
    return FALSE;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    AppWidgets *w = g_new0(AppWidgets, 1);
    w->music_playing = 0;
    w->hold_timer = 0;

    char *dir = g_path_get_dirname(argv[0]);
    snprintf(w->icon_path, sizeof(w->icon_path), "%s/headcrab.png", dir);
    snprintf(w->bin_path,  sizeof(w->bin_path),  "%s/ticket-grabber", dir);

    char bgm_uri[512], click_uri[512], hover_uri[512];
    snprintf(bgm_uri,   sizeof(bgm_uri),   "file://%s/BGM.wav",   dir);
    snprintf(click_uri, sizeof(click_uri), "file://%s/Click.mp3", dir);
    snprintf(hover_uri, sizeof(hover_uri), "file://%s/Hover.mp3", dir);
    g_free(dir);

    w->music_player = gst_element_factory_make("playbin", "music");
    if (w->music_player) g_object_set(w->music_player, "uri", bgm_uri, NULL);

    w->sfx_click = gst_element_factory_make("playbin", "sfx_click");
    if (w->sfx_click) {
        g_object_set(w->sfx_click, "uri", click_uri, NULL);
        gst_element_set_state(w->sfx_click, GST_STATE_PAUSED);
    }

    w->sfx_hover = gst_element_factory_make("playbin", "sfx_hover");
    if (w->sfx_hover) {
        g_object_set(w->sfx_hover, "uri", hover_uri, NULL);
        gst_element_set_state(w->sfx_hover, GST_STATE_PAUSED);
    }

    w->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(w->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    gtk_css_provider_load_from_data(w->css_provider, CSS, -1, NULL);

    w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w->window), "SLS-TG");
    gtk_window_set_default_size(GTK_WINDOW(w->window), 750, 420);
    gtk_window_set_resizable(GTK_WINDOW(w->window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(w->window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(w->window), 0);
    if (g_file_test(w->icon_path, G_FILE_TEST_EXISTS))
        gtk_window_set_icon_from_file(GTK_WINDOW(w->window), w->icon_path, NULL);
    g_signal_connect(w->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    w->outer_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(w->outer_frame, "outer_frame");

    w->overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(w->window), w->overlay);
    gtk_container_add(GTK_CONTAINER(w->overlay), w->outer_frame);

    w->dim_layer = gtk_event_box_new();
    gtk_widget_set_name(w->dim_layer, "dim_layer");
    gtk_widget_set_no_show_all(w->dim_layer, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(w->overlay), w->dim_layer);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(w->outer_frame), vbox);

    /* Topbar - X on the right */
    GtkWidget *topbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(topbar, "topbar");
    gtk_widget_add_events(topbar, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(topbar, "button-press-event", G_CALLBACK(on_topbar_drag), w);
    GtkWidget *spacer_top = gtk_label_new("");
    gtk_widget_set_hexpand(spacer_top, TRUE);
    gtk_box_pack_start(GTK_BOX(topbar), spacer_top, TRUE, TRUE, 0);
    w->close_btn = gtk_button_new_with_label("✕");
    gtk_widget_set_name(w->close_btn, "close_btn");
    g_signal_connect(w->close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(w->close_btn, "clicked", G_CALLBACK(on_btn_click), w);
    g_signal_connect(w->close_btn, "enter-notify-event", G_CALLBACK(on_btn_enter), w);
    gtk_box_pack_end(GTK_BOX(topbar), w->close_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), topbar, FALSE, FALSE, 0);

    /* Header - centered logo, title, subtitle */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(header, "header");
    gtk_widget_set_margin_top(header, 6);
    gtk_widget_set_margin_bottom(header, 8);

    GdkPixbuf *pb = g_file_test(w->icon_path, G_FILE_TEST_EXISTS) ?
        gdk_pixbuf_new_from_file_at_scale(w->icon_path, 60, 60, TRUE, NULL) : NULL;
    w->logo_image = pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
    gtk_widget_set_app_paintable(w->logo_image, TRUE);
    GtkWidget *event_box = gtk_event_box_new();
    gtk_widget_set_name(event_box, "logo_box");
    gtk_container_add(GTK_CONTAINER(event_box), w->logo_image);
    gtk_widget_add_events(event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_logo_press), w);
    g_signal_connect(event_box, "button-release-event", G_CALLBACK(on_logo_release), w);
    GtkWidget *logo_center = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(logo_center, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(logo_center), event_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), logo_center, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new("SLS-TG");
    gtk_widget_set_name(title, "title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.5);
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new("TICKET GRABBER");
    gtk_widget_set_name(subtitle, "subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.5);
    gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);


    /* Two-column middle section */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /* LEFT: fields */
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(left, 24);
    gtk_widget_set_margin_end(left, 16);
    gtk_widget_set_margin_top(left, 12);
    gtk_widget_set_margin_bottom(left, 12);
    gtk_widget_set_size_request(left, 355, -1);
    gtk_box_pack_start(GTK_BOX(hbox), left, FALSE, FALSE, 0);

    GtkWidget *lbl_user = gtk_label_new("USERNAME");
    gtk_widget_set_name(lbl_user, "field_label");
    gtk_widget_set_halign(lbl_user, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl_user, 8);
    gtk_box_pack_start(GTK_BOX(left), lbl_user, FALSE, FALSE, 0);
    w->entry_username = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_username), "Steam username");
    gtk_box_pack_start(GTK_BOX(left), w->entry_username, FALSE, FALSE, 0);

    GtkWidget *lbl_pass = gtk_label_new("PASSWORD");
    gtk_widget_set_name(lbl_pass, "field_label");
    gtk_widget_set_halign(lbl_pass, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl_pass, 8);
    gtk_box_pack_start(GTK_BOX(left), lbl_pass, FALSE, FALSE, 0);
    w->entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(w->entry_password), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_password), "Steam password");
    gtk_box_pack_start(GTK_BOX(left), w->entry_password, FALSE, FALSE, 0);

    GtkWidget *lbl_appid = gtk_label_new("APP ID");
    gtk_widget_set_name(lbl_appid, "field_label");
    gtk_widget_set_halign(lbl_appid, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl_appid, 8);
    gtk_box_pack_start(GTK_BOX(left), lbl_appid, FALSE, FALSE, 0);
    w->entry_appid = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_appid), "e.g. 480");
    gtk_box_pack_start(GTK_BOX(left), w->entry_appid, FALSE, FALSE, 0);


    /* RIGHT: log */
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(right, 12);
    gtk_widget_set_margin_end(right, 16);
    gtk_widget_set_margin_top(right, 12);
    gtk_widget_set_margin_bottom(right, 12);
    gtk_box_pack_start(GTK_BOX(hbox), right, TRUE, TRUE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    w->log_buf = gtk_text_buffer_new(NULL);
    w->log_view = gtk_text_view_new_with_buffer(w->log_buf);
    gtk_widget_set_name(w->log_view, "log");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(w->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(w->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(w->log_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll), w->log_view);
    gtk_box_pack_start(GTK_BOX(right), scroll, TRUE, TRUE, 0);

    /* Status below log */
    w->status_label = gtk_label_new("READY");
    gtk_widget_set_name(w->status_label, "status");
    gtk_label_set_xalign(GTK_LABEL(w->status_label), 0.5);
    gtk_box_pack_start(GTK_BOX(right), w->status_label, FALSE, FALSE, 4);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(w->log_buf, &end);
    gtk_text_buffer_insert(w->log_buf, &end,
        "[ SLS-TG INITIALIZED ]\n[ FILL IN THE FIELDS AND PRESS RUN ]", -1);


    /* Bottom section: run button centered, footer bottom left */
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(bottom, 8);
    gtk_widget_set_margin_bottom(bottom, 8);
    gtk_box_pack_start(GTK_BOX(vbox), bottom, FALSE, FALSE, 0);

    /* Run button centered */
    w->btn = gtk_button_new_with_label("▶   RUN");
    gtk_widget_set_name(w->btn, "run_btn");
    gtk_widget_set_size_request(w->btn, 200, 42);
    gtk_widget_set_halign(w->btn, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bottom), w->btn, FALSE, FALSE, 0);
    g_signal_connect(w->btn, "clicked", G_CALLBACK(on_run_clicked), w);
    g_signal_connect(w->btn, "clicked", G_CALLBACK(on_btn_click), w);
    g_signal_connect(w->btn, "enter-notify-event", G_CALLBACK(on_btn_enter), w);

    /* Footer goes directly into vbox at the very bottom */
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(footer_box, 16);
    gtk_widget_set_margin_bottom(footer_box, 6);
    gtk_box_pack_end(GTK_BOX(vbox), footer_box, FALSE, FALSE, 0);
    w->footer_link = gtk_label_new(
        "<a href='https://github.com/AceSLS/SLSsteam'>"
        "<span foreground='#aaaaaa' size='medium' underline='none'>SLSsteam</span></a>"
        " ❖ "
        "<a href='https://github.com/Ke619/SLS-TG'>"
        "<span foreground='#aaaaaa' size='medium' underline='none'>SLS-TG</span></a>");
    gtk_label_set_use_markup(GTK_LABEL(w->footer_link), TRUE);
    gtk_label_set_track_visited_links(GTK_LABEL(w->footer_link), FALSE);
    gtk_widget_set_name(w->footer_link, "footer");
    gtk_box_pack_start(GTK_BOX(footer_box), w->footer_link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bottom), footer_box, FALSE, FALSE, 0);

    gtk_widget_show_all(w->window);

    if (load_music() && w->music_player)
        start_music(w);

    gtk_main();

    if (w->music_player) {
        gst_element_set_state(w->music_player, GST_STATE_NULL);
        gst_object_unref(w->music_player);
    }
    if (w->sfx_click) {
        gst_element_set_state(w->sfx_click, GST_STATE_NULL);
        gst_object_unref(w->sfx_click);
    }
    if (w->sfx_hover) {
        gst_element_set_state(w->sfx_hover, GST_STATE_NULL);
        gst_object_unref(w->sfx_hover);
    }

    g_free(w);
    return 0;
}
