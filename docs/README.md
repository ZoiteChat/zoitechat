# ZoiteChat documentation

## Getting started

* Install [Python 3](https://www.python.org/)
* Install pip for Python 3
* Install Sphinx: `python -m pip install sphinx`
* On Windows: ensure your Python 3 installation is on `PATH`
* Run `make html` to build the HTML files
* Run the Python webserver to test your changes: `make server`
  (http://localhost:8000/)


## Conventions

* 4 Spaces are used for indentation.
* Ordered or unordered lists use visual indentation, where any non-list
  subelement would use an indentation of a multiple of 4 again.

Example:

```rest
- a list item
- a list item
  with a visually indented continuation
- a list item with a sub-list
  
  - that is also visually indented
- a list item with a code block

    .. code-block:: none

        that is not visually indented
```

## HTML theme

The `zoitechat` theme inherits Sphinx's built-in `basic` theme. It uses the
same compact layout, blue navigation, and system fonts as zoitechat.org.
Appearance starts in System mode on each page. Light and Dark are available
for the current page; no cookies or browser storage are used.

The theme's templates are in `_theme/zoitechat/`. Its readable CSS and
JavaScript are in `_static/css/zoitechat-docs.css` and
`_static/js/zoitechat-docs.js`. No minification, JavaScript package manager,
external fonts, or CDN assets are required.

Navigation URLs, including `zoite_atlas_url`, are configured through
`html_context` in `conf.py`. The logo
is `_static/zoitechat-logo.svg`. Standard Sphinx search, indexes, source
links, anchors, translations, and previous/next links remain available.
The HTML output also works as locally installed documentation; project-site
links only access the network when clicked.

Build from the repository root:

```sh
sphinx-build -b html docs docs/_build/html
```

Use a clean output directory when switching themes, so stale Basicstrap
assets from an earlier build are not included in a package.

## Debian packaging

The active theme does not use the bundled Basicstrap, Bootstrap, jQuery,
jQuery Cookie, Font Awesome, or icon fonts. Its font stack includes DejaVu
Sans and DejaVu Sans Mono; no font files are bundled by the theme.

Packagers may exclude `docs/_theme/basicstrap/` from the source package.
The default theme continues to build without that directory. No pre-minified
third-party assets need to be added or rebuilt for this theme.

Build with the distribution's `python3-sphinx`. After installing the HTML
into the package staging directory, use `dh_sphinxdoc` (or `dh --with
sphinxdoc`) to replace Sphinx's standard JavaScript files with system
symlinks. Include `${sphinxdoc:Depends}` in the package dependencies and
`${sphinxdoc:Built-Using}` where required for generated assets. The custom
`zoitechat-docs.js` remains a small, readable project source file.

A built-in theme is still selectable with `-D html_theme=classic` if a
packager needs a different layout. The old RST content has existing build
diagnostics; changing the theme does not suppress or alter them.

Shared website chrome is in `_static/css/zoitechat-site.css`, copied from the main site. Keep that readable stylesheet in sync when changing the shared header, navigation, typography, or colours.
