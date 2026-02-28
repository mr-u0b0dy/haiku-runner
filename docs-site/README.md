# haiku-runner docs site

This folder hosts project documentation using [shadcn-docs-nuxt](https://github.com/ZTL-UwU/shadcn-docs-nuxt).

## Local development

```bash
npm install
npm run dev
```

Default local URL: `http://localhost:3000`.

## Production build

```bash
npm run build
npm run preview
```

## Vercel deployment (GitHub Actions)

This repository includes [docs-vercel workflow](../.github/workflows/docs-vercel.yml) that deploys this `docs-site` project to Vercel:

- Pull requests touching `docs-site/**` → preview deployment
- Push to `main` touching `docs-site/**` → production deployment

Required GitHub repository secrets:

- `VERCEL_TOKEN`
- `VERCEL_ORG_ID`
- `VERCEL_PROJECT_ID`

You can get `VERCEL_ORG_ID` and `VERCEL_PROJECT_ID` from your local Vercel project after running:

```bash
cd docs-site
vercel link
```

Then read values from `.vercel/project.json`.
