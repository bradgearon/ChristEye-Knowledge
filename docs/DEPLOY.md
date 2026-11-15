# Publishing the ChristEye Anaiero documentation

**Updated November 15, 2026 · We Develop Software LLC**

The full project Markdown is available directly on GitHub. The professional landing page is `docs/index.html`, with styles in `docs/assets/site.css` and an accessible SVG architecture diagram.

GitHub has already reported a successful **pages build and deployment** run for the branch-based site. The default workflow does not need another custom deploy action on every commit.

## Check site status

1. Open [GitHub Pages settings](https://github.com/bradgearon/ChristEye-Knowledge/settings/pages).
2. Confirm the **publishing source** currently configured. For branch publication, use `main` and `/docs` as the folder; for GitHub Actions publication use the optional manual workflow.
3. Check [Pages build and deployment runs](https://github.com/bradgearon/ChristEye-Knowledge/actions) for the latest green result.
4. Open [https://bradgearon.github.io/ChristEye-Knowledge/](https://bradgearon.github.io/ChristEye-Knowledge/) after deployment finishes.

## Publication organization

- The GitHub Markdown library contains the six complete P01 source documents plus 174 other complete DOCX conversion editions.
- Current implementation documents live under `specifications/v3/`; historical work under `archive/v2/`.
- The original schema contracts live under `schemas/v6/` and `schemas/p02/`.
- Patent documents are not published; research ownership is covered by the WDS Public Research Reference License.

## Troubleshooting

If the website is unavailable but the Markdown works on GitHub, verify the Pages source in repository settings first. The optional `.github/workflows/pages.yml` workflow is now **manual-only**, preventing a competing release from running on every Markdown edit. A GitHub Pages build may succeed before all caching/indexing is visible to external visitors.

**The source documentation remains accessible on GitHub regardless of Pages hosting.**
