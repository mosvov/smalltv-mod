import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// Deployed to GitHub Pages at https://mosvov.github.io/smalltv-mod/
export default defineConfig({
  site: 'https://mosvov.github.io',
  base: '/smalltv-mod',
  integrations: [
    starlight({
      title: 'smalltv-mod',
      description:
        'Open-source firmware for the GeekMagic SmallTV: ticker, weather, Claude usage meter, and plane radar.',
      logo: {
        src: './src/assets/logo.svg',
        replacesTitle: false,
      },
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/mosvov/smalltv-mod',
        },
      ],
      editLink: {
        baseUrl: 'https://github.com/mosvov/smalltv-mod/edit/main/docs/',
      },
      sidebar: [
        { label: 'Home', link: '/' },
        {
          label: 'Getting started',
          items: [
            { label: 'Hardware and variants', link: '/getting-started/hardware/' },
            { label: 'Flashing', link: '/getting-started/flashing/' },
            { label: 'First-time setup', link: '/getting-started/setup/' },
          ],
        },
        {
          label: 'Features',
          items: [
            { label: 'Stock and crypto ticker', link: '/features/ticker/' },
            { label: 'Weather', link: '/features/weather/' },
            { label: 'AI usage meter', link: '/features/usage/' },
            { label: 'Plane radar', link: '/features/radar/' },
          ],
        },
        {
          label: 'Reference',
          items: [
            { label: 'Data sources', link: '/reference/data-sources/' },
            { label: 'Stock Ultra audit', link: '/reference/stock-ultra-audit/' },
            { label: 'Building from source', link: '/reference/building/' },
            { label: 'Recovery and credits', link: '/reference/recovery/' },
          ],
        },
      ],
    }),
  ],
});
