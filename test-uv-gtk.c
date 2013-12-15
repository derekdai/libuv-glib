#include <gtk/gtk.h>
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

GtkWidget *label = NULL;

int count = 0;

typedef struct _MySource MySource;

struct _MySource
{
    GSource base;

    gpointer tag;
};

void on_timeout(uv_timer_t *timer, int status)
{
    fprintf(stderr, ".");
    fflush(stderr);

    ++ count;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", count);
    gtk_label_set_text(GTK_LABEL(label), buf);
}

gboolean prepare(GSource *source, gint *timeout)
{
    uv_update_time(uv_default_loop());
    *timeout = uv_backend_timeout(uv_default_loop());

    return 0 == *timeout;
}

gboolean check(GSource *source)
{
    if(! uv_backend_timeout(uv_default_loop())) {
        return TRUE;
    }

    return G_IO_IN == g_source_query_unix_fd(source, ((MySource *) source)->tag);
}

gboolean fd_check(GSource *source)
{
    fprintf(stderr, "2");
    fflush(stderr);

    GIOCondition cond = g_source_query_unix_fd(source,
                                               ((MySource *) source)->tag);
    return G_IO_IN == cond;
}

gboolean dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);

    return TRUE;
}

void on_close(uv_handle_t *handle)
{
    free(handle);
}

void on_connect(uv_stream_t *server, int status)
{
    if(status) {
        uv_close((uv_handle_t *) server, NULL);
        return;
    }

    uv_tcp_t *conn = malloc(sizeof(uv_tcp_t));
    assert(! uv_tcp_init(uv_default_loop(), conn));

    assert(! uv_accept(server, (uv_stream_t *) conn));

    printf("Accepted new connect\n");

    uv_close((uv_handle_t *) conn, on_close);
}

int main()
{
    uv_loop_t *loop = uv_default_loop();

    GSourceFuncs source_funcs = {
        .prepare = prepare,
        .check = check,
        .dispatch = dispatch,
    };
    GSource *source = g_source_new(&source_funcs, sizeof(MySource));
    ((MySource *) source)->tag = g_source_add_unix_fd(source,
                                                      uv_backend_fd(loop),
                                                      G_IO_IN);
    g_source_attach(source, NULL);

    gtk_init(NULL, NULL);

    uv_timer_t timer;
    assert(! uv_timer_init(loop, &timer));
    assert(! uv_timer_start(&timer, on_timeout, 1000, 1000));

    uv_tcp_t server;
    assert(! uv_tcp_init(loop, &server));
    assert(! uv_tcp_init(loop, &server));
    struct sockaddr_in addr;
    assert(! uv_ip4_addr("0.0.0.0", 1234, &addr));
    assert(! uv_tcp_bind(&server, (struct sockaddr *) &addr));
    assert(! uv_listen((uv_stream_t *) &server, 10, on_connect));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(win, "destroy", gtk_main_quit, NULL);

    label = gtk_label_new("0");
    gtk_container_add(GTK_CONTAINER(win), label);

    gtk_widget_show_all(win);

    gtk_main();

    return 0;
}
