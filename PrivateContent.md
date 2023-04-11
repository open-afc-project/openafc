# Build with private contents
Private contents, e.g priv_config.py, priv_about.html, can be stored at same directory level as
open-afc, i.e. one level above src directory using git submodules.  The rat_server can be built
using rat_server/Dockerfile.submodules to slurp in the private contents during the build

e.g.
Given following directory structure
my_proj/
       open_afc/
               src
       priv_about.html
       priv_config.py

Under my_proj:
'''
docker build . --build-arg AFC_DEVEL_ENV=devel  -t rat_server_mytag --file open-afc/rat_server/Dockerfile.submodules 
'''
