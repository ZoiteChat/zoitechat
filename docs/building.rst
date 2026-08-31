Building
========

Windows
-------

The Windows build is currently x64-only and follows the same shape as
ZoiteChat's Windows GitHub Actions workflow: prepare ``C:\gtk-build``, put
the dependency bundles where ``win32\zoitechat.props`` expects them, then build
``win32\zoitechat.sln`` with MSBuild.

Software
~~~~~~~~

Install:

- `Visual Studio 2022 Community <https://visualstudio.microsoft.com/vs/community/>`_ or `Visual Studio 2022 Build Tools <https://visualstudio.microsoft.com/downloads/>`_ with the **Desktop development with C++** workload
- `Git for Windows <https://git-scm.com/download/win>`_
- `7-Zip <https://7-zip.org/>`_
- `Python 3.14 x64 <https://www.python.org/downloads/windows/>`_
- `Inno Setup 6 <https://jrsoftware.org/isdl.php>`_ if you want to build the installer

Source code
~~~~~~~~~~~

Clone the source and enter the checkout::

    git clone https://github.com/zoitechat/zoitechat.git
    cd zoitechat

Dependencies
~~~~~~~~~~~~

Create the dependency root::

    mkdir C:\gtk-build

Download the `GTK3 x64 bundle <https://github.com/ZoiteChat/gvsbuild/releases/download/zoitechat-2.18.1/GTK3_Gvsbuild_zoitechat-2.18.1_x64.zip>`_
and extract it to ``C:\gtk-build\gtk\x64\release``.

Download the remaining Windows dependency artifacts from the
`ZoiteChat gvsbuild releases <https://github.com/zoitechat/gvsbuild/releases>`_
and extract them to these paths:

- gendef: ``C:\gtk-build\gendef``
- WinSparkle x64 bundle: ``C:\gtk-build\WinSparkle\x64``
- Perl 5.42 x64 bundle: ``C:\gtk-build\perl-5.42.0.1\x64``

Set up Python where the project file expects it. From PowerShell, adjust the
Python path if your install is elsewhere::

    mkdir C:\gtk-build\python-3.14
    cmd /c mklink /J C:\gtk-build\python-3.14\x64 C:\Python314
    C:\gtk-build\python-3.14\x64\python.exe -m pip install --upgrade pip cffi

If you use different locations, set these environment variables before building
instead of editing ``win32\zoitechat.props``:

- ``ZOITECHAT_DEPS_PATH``
- ``ZOITECHAT_GENDEF_PATH``
- ``ZOITECHAT_PERL_PATH``
- ``ZOITECHAT_PYTHON3_PATH``
- ``ZOITECHAT_WINSPARKLE_PATH``
- ``ZOITECHAT_ISCC_PATH``

Building
~~~~~~~~

Open **Developer Command Prompt for VS 2022** in the source checkout and run::

    msbuild win32\zoitechat.sln /m /verbosity:minimal /p:Configuration=Release /p:Platform=x64

To build the installer, open ``win32\zoitechat.sln`` in Visual Studio,
set ``release/installer`` as the startup project, then run **Build**.

The build output is written to ``..\zoitechat-build\x64``. The installer
build writes a ``ZoiteChat*.exe`` installer in that output tree.

Unix
----

First of all, you have to install the build dependencies. Package names
differ across distros, so be creative and check meson's output
if you get an error.

Most package-managers can get the dependencies for you:

- dnf::

    dnf install -y meson gettext-devel gtk3-devel iso-codes-devel libarchive-devel libayatana-appindicator-gtk3-devel libcanberra-devel lua-devel openssl-devel perl-devel perl-ExtUtils-Embed python3-cffi python3-devel

- apt::

    apt-get install -y meson gettext iso-codes libarchive-dev libayatana-appindicator3-dev libcanberra-dev libglib2.0-dev libgtk-3-dev liblua5.4-dev libpci-dev libperl-dev libssl-dev python3-dev python3-cffi desktop-file-utils

ZoiteChat has its source code hosted using `Git <http://git-scm.com/>`_, so you have to install Git as
well. When it's ready, you can start the actual compilation, which is
basically:

.. code-block:: bash

    git clone https://github.com/zoitechat/zoitechat.git
    cd zoitechat
    meson setup build
    ninja -C build
    sudo ninja -C build install

This will compile with defaults. See ``meson configure build`` for more info
about flags.

