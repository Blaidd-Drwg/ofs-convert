#!/usr/bin/env python3
import enum
import itertools
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


NOT_ENOUGH_CLUSTERS_MSG = 'WARNING: Not enough clusters for a 32 bit FAT!'
TOOL_TIMEOUT = 5


class ToolRunner:
    def __init__(self, test_case, input_dir):
        self.input_dir = input_dir
        self.test_case = test_case
        self.collected_output = []

    def clean(self):
        err_files = self.input_dir.glob('*.err.txt')
        out_files = self.input_dir.glob('*.out.txt')
        for f in itertools.chain(err_files, out_files):
            f.unlink()

    def write_output(self):
        for out_path, output in self.collected_output:
            out_path.write_bytes(output)

    def run(self, args, name, shell=False, err_msg=None,
            custom_error_checker=None):
        try:
            proc = subprocess.run(args, shell=shell, stderr=subprocess.PIPE,
                                  stdout=subprocess.PIPE, timeout=TOOL_TIMEOUT)
        except subprocess.TimeoutExpired:
            self.test_case.fail(name + ' timed out')
        self._save_tool_output(proc.stderr, name + '.err')
        self._save_tool_output(proc.stdout, name + '.out')
        if custom_error_checker is not None:
            custom_error_checker(proc)
        else:
            self.test_case.assertEqual(
                0, proc.returncode, err_msg or name + ' did not exit cleanly')

    def _save_tool_output(self, output, stem):
        if output:
            out_path = self.input_dir / (stem + '.txt')
            self.collected_output.append((out_path, output))


class FsType(enum.Enum):
    VFAT = 0
    EXT4 = 1


class ImageMounter:
    class _Mount:
        def __init__(self, tool_runner, mount_point):
            self.tool_runner = tool_runner
            self.mount_point = mount_point

        def __enter__(self):
            return self.mount_point

        def __exit__(self, *_):
            self.tool_runner.run(
                ['umount', str(self.mount_point)], 'umount',
                err_msg='umount did not exit cleanly, check mounts')

    def __init__(self, tool_runner, temp_dir):
        self._tool_runner = tool_runner
        self._mounts_dir = temp_dir / 'mnt'
        self._mounts_dir.mkdir()

    def _make_mount_point(self, fs_type):
        mount_point = self._mounts_dir / fs_type.name.lower()
        mount_point.mkdir(exist_ok=True)
        return mount_point

    if sys.platform == 'darwin':
        class _HdiUtilMount(_Mount):
            def __exit__(self, *_):
                self.tool_runner.run(
                    ['hdiutil', 'eject', str(self.mount_point)],
                    'hdiutil eject',
                    err_msg='hdiutil eject did not exit cleanly, check mounts')

        def mount(self, image_path, fs_type, readonly):
            mount_point = self._make_mount_point(fs_type)
            if fs_type == FsType.EXT4:
                self._tool_runner.run(
                    ['ext4fuse', str(image_path), str(mount_point)], 'ext4fuse',
                    err_msg='ext4fuse mounting failed, check mounts')
                return self._Mount(self._tool_runner, mount_point)
            else:
                args = ['hdiutil', 'attach', '-imagekey',
                        'diskimage-class=CRawDiskImage', '-nobrowse']
                if readonly:
                    args.append('-readonly')
                args.extend(['-mountpoint', str(mount_point), str(image_path)])
                self._tool_runner.run(
                    args, 'hdiutil attach',
                    err_msg='hdiutil attach did not exit cleanly, check mounts')
                return self._HdiUtilMount(self._tool_runner, mount_point)
    elif sys.platform.startswith('linux'):
        def mount(self, image_path, fs_type, readonly):
            mount_point = self._make_mount_point(fs_type)
            args = ['mount', '-o', 'loop', '-t', fs_type.name.lower()]
            if readonly:
                args.append('--read-only')
            args.extend([str(image_path), str(mount_point)])
            self._tool_runner.run(
                args, 'mount',
                err_msg='mount did not exit cleanly, check mounts')
            return self._Mount(self._tool_runner, mount_point)
    else:
        raise NotImplementedError('Only implemented for macOS and Linux')


class OfsConvertTest(unittest.TestCase):
    _OFS_CONVERT = None

    @classmethod
    def setup_test_methods(cls, ofs_convert_path, tests_dir):
        cls._OFS_CONVERT = ofs_convert_path
        tests_dir = pathlib.Path(tests_dir)
        input_dirs = (e for e in tests_dir.glob('**/*.test') if e.is_dir())
        for input_dir in input_dirs:
            cls._add_test_method(input_dir)

    @classmethod
    def _add_test_method(cls, input_dir):
        fat_image_path = input_dir / 'fat.img'
        if fat_image_path.exists():
            def create_fat_image(*_args):
                return fat_image_path
        else:
            def create_fat_image(self, *args):
                return self._create_fat_image_from_gen_script(input_dir, *args)

        def test(self):
            self._run_test(input_dir, create_fat_image)

        meth_name = 'test_' + input_dir.stem.replace('-', '_')
        setattr(cls, meth_name, test)

    def _run_test(self, input_dir, create_fat_image):
        tool_runner = ToolRunner(self, input_dir)
        tool_runner.clean()
        with tempfile.TemporaryDirectory() as temp_dir_name:
            temp_dir = pathlib.Path(temp_dir_name)
            image_mounter = ImageMounter(tool_runner, temp_dir)
            try:
                fat_image_path = create_fat_image(self, temp_dir, tool_runner,
                                                  image_mounter)
                ext4_image_path = temp_dir / 'ext4.img'
                shutil.copyfile(str(fat_image_path), str(ext4_image_path))
                self._convert_to_ext4(tool_runner, ext4_image_path)
                self._run_fsck_ext4(tool_runner, ext4_image_path)
                self._check_contents(tool_runner, image_mounter, fat_image_path,
                                     ext4_image_path)
            except AssertionError:
                tool_runner.write_output()
                raise

    def _convert_to_ext4(self, tool_runner, fat_image_path):
        tool_runner.run([self._OFS_CONVERT, str(fat_image_path)], 'ofs-convert')

    def _check_fsck_ext4_error(self, proc):
        if proc.returncode & ~12 == 0:
            err_msg = 'fsck.ext4 reported errors in image'
        else:
            err_msg = 'fsck.ext4 exited with unexpected exit code'
        self.assertEqual(0, proc.returncode, err_msg)

    def _run_fsck_ext4(self, tool_runner, ext4_image_path):
        tool_runner.run(['fsck.ext4', '-n', '-f', str(ext4_image_path)],
                        'fsck.ext4',
                        custom_error_checker=self._check_fsck_ext4_error)

    def _check_rsync_errors(self, proc):
        self.assertEqual(
            b'', proc.stdout,
            'rsync reported differences between FAT and Ext4 images')

    def _check_contents(self, tool_runner, image_mounter, fat_image_path,
                        ext4_image_path):
        with image_mounter.mount(fat_image_path,
                                 FsType.VFAT, True) as fat_mount:
            with image_mounter.mount(ext4_image_path,
                                     FsType.EXT4, True) as ext_mount:
                # if the fat path doesn't end in a slash, rsync wants to copy
                # the directory and not its contents
                formatted_path_path = str(fat_mount)
                if not formatted_path_path.endswith('/'):
                    formatted_path_path += '/'
                args = ['rsync', '--dry-run', '--itemize-changes', '--archive',
                        '--checksum', '--no-perms', '--no-owner', '--no-group',
                        '--delete', '--exclude=/lost+found', formatted_path_path,
                        str(ext_mount)]
                tool_runner.run(args, 'rsync',
                                custom_error_checker=self._check_rsync_errors)

    def _check_mkfs_fat_errors(self, proc):
        self.assertEqual(0, proc.returncode, 'mkfs.fat did not exit cleanly')
        stderr = proc.stderr.decode('utf-8')
        self.assertNotIn(NOT_ENOUGH_CLUSTERS_MSG, stderr,
                         'Too few clusters specified for FAT32')

    def _create_fat_image_from_gen_script(self, input_dir, temp_dir,
                                          tool_runner, image_mounter):
        image_file_path = temp_dir / 'fat.img'
        args_file = input_dir / 'mkfs.args'
        mkfs_call = 'mkfs.fat {} {}'.format(image_file_path,
                                            args_file.read_text().rstrip('\n'))
        tool_runner.run(mkfs_call, 'mkfs.fat', shell=True,
                        custom_error_checker=self._check_mkfs_fat_errors)
        with image_mounter.mount(image_file_path,
                                 FsType.VFAT, False) as mount_point:
            tool_runner.run([str(input_dir / 'generate.sh'), str(mount_point)],
                            'gen script')
        return image_file_path


if __name__ == '__main__':
    # Executed as a script, load arguments from argv
    if len(sys.argv) != 3:
        print('Usage: {} ofs-convert tests_dir'.format(sys.argv[0]))
        exit(1)

    OfsConvertTest.setup_test_methods(sys.argv[1], sys.argv[2])
    unittest.main(argv=sys.argv[:1])
else:
    # Imported as a unittest module, load from env variables
    import os

    OfsConvertTest.setup_test_methods(os.environ['OFS_CONVERT'],
                                      os.environ['OFS_CONVERT_TESTS_DIR'])
