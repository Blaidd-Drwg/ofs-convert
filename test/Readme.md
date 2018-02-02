# Usage

Run `./test.py path/to/ofs-convert image_dir`, where `image_dir` is a directory containing `*.fat` images to test.
Requires Python 3.5+, and a `fsck.ext4` executable in `PATH`.
The FAT images will remain unchanged, all testing is done on temporary copies.

Output (stdout, stderr) of the tools used will be placed in files next to the input images (i.e. `image_dir/test.fsck-err.txt` for `image_dir/test.fat`).
No file will be created if there is no output.

`test.py` can also be used as a module for a `unittest` test runner.
In this case, the arguments must be specified using the `OFS_CONVERT` and `OFS_CONVERT_IMAGE_DIR` environment variables.
