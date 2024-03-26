# Code Format #

In order to keep the code consistent, the use of automated code formatters is requested for a PR to be accepted.  Mis-formatted code will fail a build.  You may wish to set up your editor to re-format on save depending on the editors you use.

The formatters we expect are:
For C++: [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
For python:  [pycodestyle] (https://pycodestyle.pycqa.org/en/latest/) is used to check and [autopep8](https://pypi.org/project/autopep8/) to format
For javascript: [prettier] (https://prettier.io/)


## C++ ##
Install `clang-format` on your build machine:
```
sudo apt install clang-format
```
or use docker container like silkeh/clang

Check the version:
```
clang-format --version
```
or docker based:
```
docker run --rm  silkeh/clang:14-stretch clang-format --version
```
Needed version: 14 or higher

The `clang-format` configuration file is present in root directory as `.clang-format` which is symlink to `tools/editing/clang_format_configs`.

How to format a given file (format full file):
```
clang-format -i ./<path>/<file_name>
# Ex: clang-format -i ./src/afc-engine/AfcManager.cpp
```
or docker based:
```
docker run --rm --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/open-afc -w /open-afc silkeh/clang:14-stretch clang-format -i ./src/afc-engine/AfcManager.cpp
```

How to format git staged changes:
```
git-clang-format --staged ./<path>/<file_name>
```

How to format git unstaged changes:
```
git-clang-format --force  ./<path>/<file_name>
```

## Python ##

Install pycodestyle
```
pip install pycodestyle
```
There is a configuration file [pycodestyle.cfg](pycodestyle.cfg) inside tools/editing that specifies the set up of the checks that are run against pull requests.  To run this from the root directory:

```
pycodestyle  --config=tools/editing/pycodestyle.cfg .
```

We use the autopep8 code formatter to automate making most of the fixes that are required.

```
pip install autopep8
```

To run on an individual file, also from the root:
```
 autopep8 --in-place --global-config=tools/editing/pycodestyle.cfg   -a -a <path/to/your/file.py>
```

## JavaScript ##

For JavaScript files, we use prettier.  It is installed as a dev dependency in the [package.json](../../src/web/package.json).

To run a check
```
yarn prettier src --check
```
and to run a re-format
```
yarn prettier src --write
```