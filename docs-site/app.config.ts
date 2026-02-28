export default defineAppConfig({
  shadcnDocs: {
    site: {
      name: 'haiku-runner',
      description: 'Documentation for the Zephyr-based BLE speaker scaffold and test workflows.',
    },
    theme: {
      customizable: true,
      color: 'zinc',
      radius: 0.5,
    },
    header: {
      title: 'haiku-runner docs',
      showTitle: true,
      darkModeToggle: true,
      languageSwitcher: {
        enable: false,
        triggerType: 'icon',
        dropdownType: 'select',
      },
      logo: {
        light: '/logo.svg',
        dark: '/logo-dark.svg',
      },
      nav: [],
      links: [{
        icon: 'lucide:github',
        to: 'https://github.com/mr-u0b0dy/haiku-runner',
        target: '_blank',
      }],
    },
    aside: {
      useLevel: true,
      collapse: false,
    },
    main: {
      breadCrumb: true,
      showTitle: true,
    },
    footer: {
      credits: 'Copyright © 2026',
      links: [{
        icon: 'lucide:github',
        to: 'https://github.com/mr-u0b0dy/haiku-runner',
        target: '_blank',
      }],
    },
    toc: {
      enable: true,
      links: [{
        title: 'Open Issues',
        icon: 'lucide:circle-dot',
        to: 'https://github.com/mr-u0b0dy/haiku-runner/issues',
        target: '_blank',
      }],
    },
    search: {
      enable: true,
      inAside: false,
    }
  }
});