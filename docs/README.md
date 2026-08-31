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

## Read the Docs theme skin

The HTML output keeps the existing `basicstrap` Sphinx theme, then applies the
ZoiteChat main-site skin from `_static/css/zoitechat-docs.css`.

Brand navigation links are set in `conf.py` through `html_context`, and the
header logo uses `_static/zoitechat-logo.svg`.
