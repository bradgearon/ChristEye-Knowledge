# Enable the documentation site

The documentation source and responsive website are committed in `docs/`. GitHub Actions can deploy them as static pages **after the repository owner enables GitHub Pages**.

1. Open [ChristEye-Knowledge Pages settings](https://github.com/bradgearon/ChristEye-Knowledge/settings/pages).
2. Set **Build and deployment → Source = GitHub Actions** and save.
3. Open [Deploy Anaiero GitHub Pages](https://github.com/bradgearon/ChristEye-Knowledge/actions/workflows/pages.yml) and choose **Run workflow** on `main` if a new run does not start automatically.
4. Confirm the workflow is green before using `https://bradgearon.github.io/ChristEye-Knowledge/`.

The previous attempt failed at `actions/configure-pages` because the site was not enabled. The workflow token was not permitted to create the Pages site automatically. **Files are already available in the GitHub repository independent of Pages.**

The web landing page is `docs/index.html`, which references `docs/assets/site.css` and `docs/assets/architecture.svg`. It does not expose raw logs or patent materials.
