/**
 * Minimal Worker entry point for the Anaiero static React documentation portal.
 *
 * Cloudflare serves matching static assets directly, without invoking this
 * script. The entry point exists to give this deployment an explicit script
 * and reserve /api/* for a future opt-in interest-registration service.
 * No database, live signup, or biological equipment connection exists here.
 */
export default {
  async fetch(request, env) {
    const pathname = new URL(request.url).pathname;
    if (pathname.startsWith('/api/')) {
      return new Response(JSON.stringify({
        error: 'This API is not enabled.',
      }), {
        status: 404,
        headers: {
          'content-type': 'application/json; charset=utf-8',
          'cache-control': 'no-store',
        },
      });
    }
    return env.ASSETS.fetch(request);
  },
};
