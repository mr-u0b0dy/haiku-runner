import { computeRoute, inject, pageview } from '@vercel/analytics'
import type { RouteLocationNormalizedLoaded } from 'vue-router'

export default defineNuxtPlugin((nuxtApp) => {
  inject({
    framework: 'nuxt',
    disableAutoTrack: true,
  })

  const sendPageview = (route: RouteLocationNormalizedLoaded) => {
    pageview({
      route: computeRoute(route.path, route.params as Record<string, string | string[]>),
      path: route.path,
    })
  }

  sendPageview(nuxtApp.$router.currentRoute.value)
  nuxtApp.$router.afterEach((to) => {
    sendPageview(to)
  })
})
