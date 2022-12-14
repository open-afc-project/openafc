# Code Format

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
docker run --rm -v `pwd`:/open-afc -w /open-afc silkeh/clang:14-stretch clang-format -i ./src/afc-engine/AfcManager.cpp
```

How to format git staged changes:
```
git-clang-format --staged ./<path>/<file_name>
```

How to format git unstaged changes:
```
git-clang-format --force  ./<path>/<file_name>
```
