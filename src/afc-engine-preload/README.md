This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

# Table of Contents
- [Table of Contents](#table-of-contents)
- [The afc-engine preload library](#the-afc-engine-preload-library)
# The afc-engine preload library
The afc-engine preload library redirects read accesses to the geospatial data. The access can be redirected to other local directory or to remote storage. The redirection is implemented by overwriting file access syscalls.

The preload library also enables the local cache of geospatial data files.

This library requeres a file tree info of the redirected directory which is created by src/afc-engine-preload/parse_fs.py script:
```
src/afc-engine-preload/parse_fs.py <redirected_dir_path> <file_tree_info_output>
```

The library could be configured by the following env vars in the worker docker:
- **AFC_AEP_ENABLE**=any_value - Enable the library if defined. Default: Not defined.
- **AFC_AEP_FILELIST**=path - Path to file tree info file. Default: /aep/list/aep.list
- **AFC_AEP_DEBUG**=number - Log level. 0 - disable, 1 - log time of read operations. Default: 0
- **AFC_AEP_LOGFILE**=path - Where to write the log. Default:  /aep/log/aep.log
- **AFC_AEP_CACHE**=path - Where to store the cache. Default:  /aep/cache
- **AFC_AEP_CACHE_MAX_FILE_SIZE**=`size`- Cache files with size less than `size`. Default: 60.000.000 bytes
- **AFC_AEP_CACHE_MAX_SIZE**=`size`- Max cache size. Default: 1.000.000.000 bytes
- **AFC_AEP_REAL_MOUNTPOINT**=path - Redirect read access to there. Default: /mnt/nfs/rat_transfer
- **AFC_AEP_ENGINE_MOUNTPOINT**=path - Redirect read access from here. Default: the value of **AFC_AEP_REAL_MOUNTPOINT** var
