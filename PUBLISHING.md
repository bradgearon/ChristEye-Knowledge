# Publish the ChristEye Anaiero documentation safely

This working copy contains a **private_archive/** folder with original long-form app logs that could include private conversation, debugging payloads, local addresses, source context, and other sensitive records. The directory is intentionally listed in `.gitignore`; do not force-add it to a public repository.

## Recommended publication path

1. Review the release as a local file archive before committing. Read `private_archive/README.md`.
2. Create a GitHub repository (prefer private until you review licensing, intellectual property, and user content).
3. Copy the files other than `private_archive/` into it, or extract the separate `anaiero_public_site_v6.zip` instead.
4. Run `python website/build_site.py` and `python -m pytest -q`; inspect the generated docs site in `website/site/index.html`.
5. Commit to the repository's `main` branch. In GitHub Settings → Pages, set the source to **GitHub Actions**. The workflow in `.github/workflows/pages.yml` publishes `website/site/` only.
6. Review the public site before changing repository visibility. No raw app transcript or user-private source should be served on GitHub Pages.

No GitHub repository has been modified or deployed from this package. A code archive, even with successful tests, is not an approved PCB design, laboratory procedure or validated biological computer.