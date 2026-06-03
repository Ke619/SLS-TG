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
    GstElement *music_player;
    GstElement *sfx_click;
    GstElement *sfx_hover;
    GstElement *sfx_ticket;
    int music_playing;
    guint hold_timer;
    guint dot_timer;
    int dot_count;
    int error_set;
    char bin_path[512];
    char icon_path[512];
    GdkPixbuf *bg_pixbuf;
    char logo_idle[512];
    char logo_processing[512];
    char logo_success[512];
    char logo_error[512];
} AppWidgets;

static const char *CSS =
    "window { background-color: #000000; }"
    "image { background-color: transparent; }"
    "#logo_box { background-color: transparent; }"
    "#outer_frame { background-color: transparent; }"
    "#title { color: #cc2200; font-size: 22px; font-weight: bold; letter-spacing: 4px; }"
    "#subtitle { color: #aaaaaa; font-size: 10px; letter-spacing: 5px; }"
    "#run_btn { background: rgba(0,0,0,0.4); color: #ffffff; border: 2px solid #ffffff;"
    "  font-size: 15px; font-weight: bold; letter-spacing: 3px; padding: 10px 40px; border-radius: 50px; }"
    "#run_btn:hover { background-color: rgba(0,0,0,0.55); color: #ffffff; }"
    "#run_btn:active { background-color: rgba(0,0,0,0.7); color: #ffffff; }"
    "#run_btn:disabled { background-color: transparent; color: #888; border-color: #888; }"
    "#close_btn { background: #cc2200; color: #ffffff; border: 2px solid #cc2200; margin-bottom: 6px; margin-right: 6px; border-radius: 50%;"
    "  font-size: 11px; font-weight: bold; padding: 0; min-width: 20px; min-height: 20px; }"
    "#close_btn:hover { background: #ff3300; color: #ffffff; border-color: #ff3300; }"
    "#close_btn:active { background: #880000; color: #ffffff; border-color: #880000; }"
    "#info_btn { background: #5dade2; color: #ffffff; border: 2px solid #5dade2; border-radius: 50%; font-size: 11px; font-weight: bold; padding: 0; min-width: 22px; min-height: 22px; -gtk-outline-radius: 50%; }"
    "#info_btn:hover { background: #85c1e9; border-color: #85c1e9; }"
    "#info_btn:active { background: #2e86c1; border-color: #2e86c1; }"
    "#info_label { color: #ffffff; font-size: 11px; font-weight: bold; }"
    "#topbar { background-color: transparent; }"
    "#header { background-color: transparent; }"
    "#status { color: #e6cc00; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#status_done { color: #228822; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#status_error { color: #ff3300; font-size: 14px; font-weight: bold; letter-spacing: 2px; }"
    "#log { background-color: #000000; color: #aaaaaa; font-family: monospace; font-size: 12px; }"
    "#log text { background-color: #000000; color: #aaaaaa; }"
    "scrolledwindow { }"
    "#field_label { color: #aaaaaa; font-size: 11px; letter-spacing: 2px; }"
    "entry { background-color: #ffffff; color: #000000; border: 1px solid #000000;"
    "  border-radius: 0; padding: 6px 10px; font-size: 13px; min-height: 32px; }"
    "entry:focus { border-color: #000000; background-color: #ffffff; color: #000000; }"
    "entry { -gtk-icon-source: none; }"
    "entry.password { font-family: monospace; }"
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

static void set_logo(AppWidgets *w, const char *path) {
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) return;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, 400, 400, TRUE, NULL);
    if (pb) gtk_image_set_from_pixbuf(GTK_IMAGE(w->logo_image), pb);
}

typedef struct { AppWidgets *w; char path[512]; } LogoUpdate;

static gboolean apply_logo(gpointer data) {
    LogoUpdate *lu = (LogoUpdate *)data;
    set_logo(lu->w, lu->path);
    free(lu);
    return G_SOURCE_REMOVE;
}

static void update_logo(AppWidgets *w, const char *path) {
    LogoUpdate *lu = malloc(sizeof(LogoUpdate));
    lu->w = w;
    strncpy(lu->path, path, sizeof(lu->path)-1);
    g_idle_add(apply_logo, lu);
}

static gboolean on_done(gpointer data) {
    char **args = (char **)data;
    AppWidgets *w = (AppWidgets *)args[0];
    int code = atoi(args[1]);
    gtk_widget_set_sensitive(w->btn, TRUE);
    if (w->dot_timer) { g_source_remove(w->dot_timer); w->dot_timer = 0; }
    if (code == 0) {
        gtk_label_set_text(GTK_LABEL(w->status_label), "TICKET GENERATED!");
        gtk_widget_set_name(w->status_label, "status_done");
        set_logo(w, w->logo_success);
        play_sfx(w->sfx_ticket);
    } else if (!w->error_set) {
        gtk_label_set_text(GTK_LABEL(w->status_label), "CRITICAL ERROR");
        gtk_widget_set_name(w->status_label, "status_error");
        set_logo(w, w->logo_error);
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

typedef struct { AppWidgets *w; char status[256]; int is_error; int stop_anim; } StatusUpdate;

static gboolean apply_status(gpointer data) {
    StatusUpdate *su = (StatusUpdate *)data;
    AppWidgets *w = su->w;
    if (su->stop_anim && w->dot_timer) {
        g_source_remove(w->dot_timer);
        w->dot_timer = 0;
    }
    gtk_label_set_text(GTK_LABEL(w->status_label), su->status);
    if (su->is_error)
        gtk_widget_set_name(w->status_label, "status_error");
    else if (strcmp(su->status, "TICKET GENERATED!") == 0)
        gtk_widget_set_name(w->status_label, "status_done");
    else
        gtk_widget_set_name(w->status_label, "status");
    if (su->is_error) {
        w->error_set = 1;
        gtk_widget_set_sensitive(w->btn, TRUE);
        set_logo(w, w->logo_error);
    } else if (strcmp(su->status, "TICKET GENERATED!") == 0) {
        set_logo(w, w->logo_success);
    } else {
        set_logo(w, w->logo_processing);
    }
    free(su);
    return G_SOURCE_REMOVE;
}

static void update_status(AppWidgets *w, const char *status, int is_error, int stop_anim) {
    StatusUpdate *su = malloc(sizeof(StatusUpdate));
    su->w = w;
    strncpy(su->status, status, sizeof(su->status)-1);
    su->is_error = is_error;
    su->stop_anim = stop_anim;
    g_idle_add(apply_status, su);
}

static gpointer run_thread(gpointer data) {
    ThreadData *td = (ThreadData *)data;
    AppWidgets *w = td->w;
    log_from_thread(w, "[ LAUNCHING TICKET-GRABBER... ]");
    FILE *fp = popen(td->cmd, "r");
    free(td);
    if (!fp) {
        log_from_thread(w, "[ ERROR: failed to launch ]");
        update_status(w, "INTERNAL ERROR", 1, 1);
        done_from_thread(w, 1);
        return NULL;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        log_from_thread(w, line);

        if (strstr(line, "Connected to Steam")) {
            update_status(w, "AWAITING STEAM GUARD AUTHENTICATION", 0, 1);
        } else if (strstr(line, "Logged in as")) {
        } else if (strstr(line, "Account Info received")) {
            update_status(w, "GENERATING YOUR TICKET.", 0, 0);

        } else if (strstr(line, "Saved")) {
            update_status(w, "TICKET GENERATED!", 0, 1);
        } else if (strstr(line, "is not a number")) {
            update_status(w, "INVALID APP ID", 1, 1);
        } else if (strstr(line, "Failed to get Handlers")) {
            update_status(w, "INTERNAL ERROR", 1, 1);
        } else if (strstr(line, "Failed GetAppOwnershipTicket")) {
            update_status(w, "OWNERSHIP VERIFICATION FAILED", 1, 1);
        } else if (strstr(line, "Failed RequestEncryptedAppTicket")) {
            update_status(w, "TICKET ENCRYPTION FAILED", 1, 1);
        } else if (strstr(line, "Failed to receive both tickets")) {
            update_status(w, "STEAM CONNECTION ERROR", 1, 1);
        } else if (strstr(line, "Disconnected from Steam")) {
            update_status(w, "DISCONNECTED FROM STEAM", 1, 1);
        }
    }
    int ret = pclose(fp);
    done_from_thread(w, WEXITSTATUS(ret));
    return NULL;
}

static gboolean on_dot_tick(gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    w->dot_count = (w->dot_count % 3) + 1;
    /* Get current label to keep the prefix and just update dots */
    const char *current = gtk_label_get_text(GTK_LABEL(w->status_label));
    char prefix[64];
    strncpy(prefix, current, sizeof(prefix));
    /* Strip trailing dots */
    int len = strlen(prefix);
    while (len > 0 && prefix[len-1] == '.') { prefix[--len] = 0; }
    char label[128];
    snprintf(label, sizeof(label), "%s%.*s", prefix, w->dot_count, "...");
    gtk_label_set_text(GTK_LABEL(w->status_label), label);
    gtk_widget_set_name(w->status_label, "status");
    return G_SOURCE_CONTINUE;
}

static void on_run_clicked(GtkWidget *btn, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(w->entry_username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(w->entry_password));
    const char *appid    = gtk_entry_get_text(GTK_ENTRY(w->entry_appid));
    if (strlen(username) == 0 || strlen(password) == 0 || strlen(appid) == 0) {
        gtk_label_set_text(GTK_LABEL(w->status_label), "REQUIRED DETAILS MISSING");
        gtk_widget_set_name(w->status_label, "status_error");
        return;
    }
    gtk_widget_set_sensitive(w->btn, FALSE);
    w->dot_count = 0;
    /* Start with CONNECTING animation */
    gtk_label_set_text(GTK_LABEL(w->status_label), "CONNECTING.");
    w->dot_timer = g_timeout_add(500, on_dot_tick, w);
    gtk_widget_set_name(w->status_label, "status");
    w->error_set = 0;
    set_logo(w, w->logo_processing);
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

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    if (!w->bg_pixbuf) return FALSE;
    int win_w = gtk_widget_get_allocated_width(widget);
    int win_h = gtk_widget_get_allocated_height(widget);
    int border = 5;
    int bottom_border = 45;
    int inner_w = win_w - border * 2;
    int inner_h = win_h - border - bottom_border;
    /* Fill whole widget black first */
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);
    /* Draw bg inside the border */
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(w->bg_pixbuf, inner_w, inner_h, GDK_INTERP_BILINEAR);
    if (scaled) {
        gdk_cairo_set_source_pixbuf(cr, scaled, border, border);
        cairo_paint(cr);
        g_object_unref(scaled);
    }
    return FALSE;
}

static gboolean on_info_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppWidgets *w = (AppWidgets *)data;
    play_sfx(w->sfx_click);
    gtk_show_uri_on_window(GTK_WINDOW(w->window), "https://github.com/Ke619/SLS-TG", GDK_CURRENT_TIME, NULL);
    return FALSE;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    AppWidgets *w = g_new0(AppWidgets, 1);
    w->music_playing = 0;
    w->hold_timer = 0;

    char *dir = g_path_get_dirname(argv[0]);
    char saved_dir[512];
    snprintf(saved_dir, sizeof(saved_dir), "%s", dir);
    snprintf(w->icon_path, sizeof(w->icon_path), "%s/L0.png", dir);
    snprintf(w->logo_idle, sizeof(w->logo_idle), "%s/L0.png", dir);
    snprintf(w->logo_processing, sizeof(w->logo_processing), "%s/L1.png", dir);
    snprintf(w->logo_success, sizeof(w->logo_success), "%s/L3.png", dir);
    snprintf(w->logo_error, sizeof(w->logo_error), "%s/L2.png", dir);
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

    char ticket_uri[512];
    snprintf(ticket_uri, sizeof(ticket_uri), "file://%s/ticket.mp3", saved_dir);
    w->sfx_ticket = gst_element_factory_make("playbin", "sfx_ticket");
    if (w->sfx_ticket) {
        g_object_set(w->sfx_ticket, "uri", ticket_uri, NULL);
        gst_element_set_state(w->sfx_ticket, GST_STATE_PAUSED);
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
    gtk_window_set_default_size(GTK_WINDOW(w->window), 450, 620);
    gtk_window_set_resizable(GTK_WINDOW(w->window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(w->window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(w->window), 0);
    if (g_file_test(w->icon_path, G_FILE_TEST_EXISTS))
        gtk_window_set_icon_from_file(GTK_WINDOW(w->window), w->icon_path, NULL);
    g_signal_connect(w->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Load background image */
    char bg_path[512];
    snprintf(bg_path, sizeof(bg_path), "%s/Bg.png", saved_dir);
    w->bg_pixbuf = g_file_test(bg_path, G_FILE_TEST_EXISTS) ?
        gdk_pixbuf_new_from_file(bg_path, NULL) : NULL;
    gtk_widget_set_app_paintable(w->window, TRUE);
    g_signal_connect(w->window, "draw", G_CALLBACK(on_draw), w);

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
    /* close button moved to bottom right */
    gtk_box_pack_start(GTK_BOX(vbox), topbar, FALSE, FALSE, 0);

    /* Header - centered logo, title, subtitle */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(header, "header");
    gtk_widget_set_margin_top(header, 6);
    gtk_widget_set_margin_bottom(header, 8);

    /* Logo image widget */
    GdkPixbuf *pb0 = g_file_test(w->icon_path, G_FILE_TEST_EXISTS) ?
        gdk_pixbuf_new_from_file_at_scale(w->icon_path, 400, 400, TRUE, NULL) : NULL;
    w->logo_image = pb0 ? gtk_image_new_from_pixbuf(pb0) : gtk_image_new();
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

    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);


    /* Single column layout */
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(left, 24);
    gtk_widget_set_margin_end(left, 24);
    gtk_widget_set_margin_top(left, 8);
    gtk_box_pack_start(GTK_BOX(vbox), left, FALSE, FALSE, 0);

    /* Field group helper: each group is a fixed-height box */
    /* USERNAME */
    GtkWidget *grp_user = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    w->entry_username = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_username), "Steam username");
    gtk_widget_set_size_request(w->entry_username, -1, 36);
    gtk_box_pack_start(GTK_BOX(grp_user), w->entry_username, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), grp_user, FALSE, FALSE, 4);

    /* PASSWORD */
    GtkWidget *grp_pass = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    w->entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(w->entry_password), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_password), "Steam password");
    gtk_widget_set_size_request(w->entry_password, -1, 36);
    gtk_box_pack_start(GTK_BOX(grp_pass), w->entry_password, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), grp_pass, FALSE, FALSE, 4);

    /* APP ID */
    GtkWidget *grp_appid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    w->entry_appid = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(w->entry_appid), "App ID");
    gtk_widget_set_size_request(w->entry_appid, -1, 36);
    gtk_box_pack_start(GTK_BOX(grp_appid), w->entry_appid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left), grp_appid, FALSE, FALSE, 4);


    /* Log below fields */
    /* Log hidden - buffers kept for future use */
    w->log_buf = gtk_text_buffer_new(NULL);
    w->log_view = gtk_text_view_new_with_buffer(w->log_buf);

    /* Status below log */
    w->status_label = gtk_label_new("");
    gtk_widget_set_name(w->status_label, "status");
    gtk_label_set_xalign(GTK_LABEL(w->status_label), 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), w->status_label, FALSE, FALSE, 4);
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
    w->btn = gtk_button_new_with_label("GENERATE");
    gtk_widget_set_name(w->btn, "run_btn");
    gtk_widget_set_size_request(w->btn, 200, 42);
    gtk_widget_set_halign(w->btn, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(bottom), w->btn, FALSE, FALSE, 0);
    g_signal_connect(w->btn, "clicked", G_CALLBACK(on_run_clicked), w);
    g_signal_connect(w->btn, "clicked", G_CALLBACK(on_btn_click), w);
    g_signal_connect(w->btn, "enter-notify-event", G_CALLBACK(on_btn_enter), w);



    /* Footer goes directly into vbox at the very bottom */
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    /* Close button bottom right */

    GtkWidget *info_box = gtk_event_box_new();
    gtk_widget_set_name(info_box, "info_btn");
    gtk_widget_set_size_request(info_box, 22, 22);
    gtk_widget_add_events(info_box, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK);
    g_signal_connect(info_box, "button-press-event", G_CALLBACK(on_info_clicked), w);
    g_signal_connect(info_box, "enter-notify-event", G_CALLBACK(on_btn_enter), w);
    GtkWidget *info_label = gtk_label_new("i");
    gtk_widget_set_name(info_label, "info_label");
    gtk_container_add(GTK_CONTAINER(info_box), info_label);
    gtk_box_pack_start(GTK_BOX(footer_box), info_box, FALSE, FALSE, 0);

    /* Bottom overlay row: footer + X button pinned to bottom of window */
    GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(bottom_row, GTK_ALIGN_FILL);
    gtk_widget_set_valign(bottom_row, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(bottom_row, 6);
    gtk_widget_set_margin_start(bottom_row, 8);
    gtk_widget_set_margin_end(bottom_row, 8);

    gtk_box_pack_start(GTK_BOX(bottom_row), footer_box, TRUE, TRUE, 0);

    w->close_btn = gtk_button_new_with_label("✕");
    gtk_widget_set_name(w->close_btn, "close_btn");
    g_signal_connect(w->close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(w->close_btn, "clicked", G_CALLBACK(on_btn_click), w);
    g_signal_connect(w->close_btn, "enter-notify-event", G_CALLBACK(on_btn_enter), w);
    gtk_box_pack_end(GTK_BOX(bottom_row), w->close_btn, FALSE, FALSE, 0);

    gtk_overlay_add_overlay(GTK_OVERLAY(w->overlay), bottom_row);

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
    if (w->sfx_ticket) {
        gst_element_set_state(w->sfx_ticket, GST_STATE_NULL);
        gst_object_unref(w->sfx_ticket);
    }

    g_free(w);
    return 0;
}
