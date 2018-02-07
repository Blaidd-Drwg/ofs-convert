# Usage

Run `./run.py path/to/ofs-convert tests_dir`, where `tests_dir` is a directory which can contain:
 * `*.fat` images to test.
   Testing will be done on temporary copies.
 * `*.sh` generating scripts, with accompanying `*.mkfs` files.
   The shell script will be executed with a path to a mounted FAT partition and should generate test data inside the partition.
   It should be set to be executable.
   The `.mkfs` file should contain the arguments to `mkfs.fat`, excluding the path to the image file which will be set by `run.py`.
   It should always contain:
     - `-F 32`, to select FAT32 mode
     - `-s ...`, to select sectors per cluster (creating cluster 1k or larger)
     - `-S ...`, to select a sector size
     - `-C`, to create a new file
     - and, as the last argument, the number of 1k blocks in the created image file.
       The minimum number of blocks is 66055 + 1 for 1k clusters, 132110 + 1 for 2k clusters, etc.

Requires:
 * Python 3.5+
 * `fsck.ext4`
 * `mkfs.fat`
 * `rsync`
 * support for mounting `vfat` and `ext4` partitions using `mount` (on Linux)
 * `ext4fuse` (on macOS)

If a test fails, the output (stdout, stderr) of tools will be placed in files next to the input images (i.e. `image_dir/test.fsck.ext4.err.txt` for `image_dir/test.fat`).
No file will be created if there is no output.

`run.py` can also be used as a module for a `unittest` test runner.
In this case, the arguments must be specified using the `OFS_CONVERT` and `OFS_CONVERT_TESTS_DIR` environment variables.
