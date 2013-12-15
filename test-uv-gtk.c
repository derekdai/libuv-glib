#include <gtk/gtk.h>
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define UV_SOURCE(s) ((UvSource *) (s))

typedef struct _UvSource UvSource;

struct _UvSource
{
    GSource base;

    gpointer tag;

    uv_loop_t *loop;
};

static gboolean uv_source_prepare(GSource *source, gint *timeout)
{
    uv_update_time(uv_default_loop());
    *timeout = uv_backend_timeout(uv_default_loop());

    return 0 == *timeout;
}

static gboolean uv_source_check(GSource *source)
{
    if(! uv_backend_timeout(uv_default_loop())) {
        return TRUE;
    }

    return G_IO_IN == g_source_query_unix_fd(source,
                                             UV_SOURCE(source)->tag);
}

static gboolean uv_source_dispatch(GSource *source,
                                   GSourceFunc callback,
                                   gpointer user_data)
{
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);

    return TRUE;
}

static void uv_source_finalize(GSource *source)
{
    g_source_remove_unix_fd(source, UV_SOURCE(source)->tag);
}

GSource * uv_source_new(uv_loop_t *loop)
{
    g_return_val_if_fail(NULL != loop, NULL);

    static GSourceFuncs funcs = {
        .prepare = uv_source_prepare,
        .check = uv_source_check,
        .dispatch = uv_source_dispatch,
        .finalize = uv_source_finalize
    };

    GSource *self = g_source_new(&funcs, sizeof(UvSource));
    UV_SOURCE(self)->loop = loop;
    UV_SOURCE(self)->tag = g_source_add_unix_fd(self,
                                                uv_backend_fd(loop),
                                                G_IO_IN);
    return self;
}

guint uv_source_add(uv_loop_t *loop)
{
    g_return_val_if_fail(NULL != loop, 0);

    GSource *self = uv_source_new(loop);
    if(! self) {
        return 0;
    }

    guint tag = g_source_attach(self, NULL);
    g_source_unref(self);

    return tag;
}

GtkWidget *label = NULL;

int count = 0;

void on_timeout(uv_timer_t *timer, int status)
{
    ++ count;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", count);
    gtk_label_set_text(GTK_LABEL(label), buf);
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

void on_signal(uv_signal_t *sig, int signum)
{
    gtk_main_quit();
}

int main(int argc, char *args[])
{
    gtk_init(&argc, &args);

    uv_loop_t *loop = uv_default_loop();
    guint tag = uv_source_add(loop);

    uv_timer_t timer;
    assert(! uv_timer_init(loop, &timer));
    assert(! uv_timer_start(&timer, on_timeout, 10, 10));

    uv_signal_t sig;
    assert(! uv_signal_init(loop, &sig));
    assert(! uv_signal_start(&sig, on_signal, SIGINT));

    /* listen on port 1234, try connect with "nc localhost 1234" */
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

    uv_close((uv_handle_t *) &server, NULL);
    uv_close((uv_handle_t *) &sig, NULL);
    uv_close((uv_handle_t *) &timer, NULL);
    g_source_remove(tag);

    /* TODO if terminate with ctrl+c, uv_loop_delete() blocks */
    /*uv_loop_delete(loop);*/

    return 0;
}
