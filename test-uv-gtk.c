#include <gtk/gtk.h>
#include <uv.h>
#include <stdio.h>
#include <assert.h>

void on_timeout(uv_timer_t *timer, int status)
{
    printf(".");
}

int main()
{
    gtk_init(NULL, NULL);

    uv_loop_t *loop = uv_default_loop();
    uv_timer_t timer;
    assert(! uv_timer_init(loop, &timer));
    assert(! uv_timer_start(&timer, on_timeout, 1000, 1000));

    gtk_main();

    return 0;
}
