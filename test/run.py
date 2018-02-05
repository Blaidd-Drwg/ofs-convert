#!/usr/bin/env python3
import itertools
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


NOT_ENOUGH_CLUSTERS_MSG = 'WARNING: Not enough clusters for a 32 bit FAT!'
TOOL_TIMEOUT = 5


class OfsConvertTest(unittest.TestCase):
    @classmethod
    def _add_test_method(cls, input_path):
        if input_path.suffix == '.fat':
            conversion_cls = ImageConversionTester
        else:
            conversion_cls = GenerateScriptConversionTester

        def test(self):
            conversion_cls(self, input_path).run()

        meth_name = 'test_' + input_path.stem.replace('-', '_')
        setattr(cls, meth_name, test)

    @classmethod
    def setup_test_methods(cls, ofs_convert_path, tests_dir):
        ConversionTester.OFS_CONVERT = ofs_convert_path
        tests_dir = pathlib.Path(tests_dir)
        images = tests_dir.glob('**/*.fat')
        generation_scripts = tests_dir.glob('**/*.sh')
        for input_path in itertools.chain(images, generation_scripts):
            cls._add_test_method(input_path)


class ConversionTester:
    OFS_CONVERT = None

    def __init__(self, test_case, input_path):
        self.test_case = test_case
        self.input_path = input_path
        self.tool_outputs = []

    def run(self):
        self._clean_err_out_files()
        try:
            self._run_test()
        except AssertionError:
            self._write_tool_output()
            raise

    def _run_test(self):
        raise NotImplementedError

    def _save_tool_output(self, output, suffix):
        if output:
            name = '{}.{}.txt'.format(self.input_path.stem, suffix)
            out_path = self.input_path.parent / name
            self.tool_outputs.append((out_path, output))

    def _write_tool_output(self):
        for out_path, output in self.tool_outputs:
            out_path.write_bytes(output)

    def _run_tool(self, args, name, shell=False, err_msg=None,
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

    def _clean_err_out_files(self):
        directory = self.input_path.parent
        prefix = self.input_path.stem

        err_files = directory.glob(prefix + '*.err.txt')
        out_files = directory.glob(prefix + '*.out.txt')
        for f in itertools.chain(err_files, out_files):
            f.unlink()

    def _convert_to_ext4(self, fat_image_path):
        self._run_tool([self.OFS_CONVERT, str(fat_image_path)], 'conversion')

    def _check_fsck_ext4_error(self, proc):
        if proc.returncode & ~12 == 0:
            err_msg = 'fsck.ext4 reported errors in image'
        else:
            err_msg = 'fsck.ext4 exited with unexpected exit code'
        self.test_case.assertEqual(0, proc.returncode, err_msg)

    def _check_ext4_image(self, ext4_image_path):
        self._run_tool(['fsck.ext4', '-n', '-f', str(ext4_image_path)],
                       'fsck.ext4',
                       custom_error_checker=self._check_fsck_ext4_error)

    if sys.platform == 'darwin':
        def _mount_fat(self, image_path, mount_point):
            mount_point.mkdir(exist_ok=True)
            args = ['hdiutil', 'attach', '-imagekey',
                    'diskimage-class=CRawDiskImage', '-nobrowse', '-mountpoint',
                    str(mount_point), str(image_path)]
            self._run_tool(
                args, 'hdiutil attach',
                err_msg='hdiutil attach did not exit cleanly, check mounts')

        def _umount(self, mount_point):
            args = ['hdiutil', 'eject', str(mount_point)]
            self._run_tool(
                args, 'hdiutil eject',
                err_msg='hdiutil eject did not exit cleanly, check mounts')
    elif sys.platform.startswith('linux'):
        def _mount_fat(self, image_path, mount_point):
            mount_point.mkdir(exist_ok=True)
            args = ['mount', '-o', 'loop', '-t', 'vfat', str(image_path), str(mount_point)]
            self._run_tool(args, 'mount',
                           err_msg='mount did not exit cleanly, check mounts')

        def _umount(self, mount_point):
            self._run_tool(['umount', str(mount_point)], 'umount',
                           err_msg='umount did not exit cleanly, check mounts')
    else:
        raise NotImplementedError('Only works on macOS and Linux')


class ImageConversionTester(ConversionTester):
    def _run_test(self):
        with tempfile.NamedTemporaryFile() as temp_file:
            with self.input_path.open('rb') as src_file:
                shutil.copyfileobj(src_file, temp_file)
            temp_file.flush()
            temp_file_path = pathlib.Path(temp_file.name)
            self._convert_to_ext4(temp_file_path)
            self._check_ext4_image(temp_file_path)


class GenerateScriptConversionTester(ConversionTester):
    def _check_mkfs_fat_errors(self, proc):
        self.test_case.assertEqual(0, proc.returncode,
                                   'mkfs.fat did not exit cleanly')
        stderr = proc.stderr.decode('utf-8')
        self.test_case.assertNotIn(NOT_ENOUGH_CLUSTERS_MSG, stderr,
                                   'Too few clusters specified for FAT32')

    def _create_fat_image(self, temp_dir):
        image_file_path = temp_dir / 'fat.img'

        args_file = self.input_path.parent / (self.input_path.stem + '.mkfs')
        mkfs_call = 'mkfs.fat {} {}'.format(image_file_path,
                                            args_file.read_text().rstrip('\n'))
        self._run_tool(mkfs_call, 'mkfs.fat', shell=True,
                       custom_error_checker=self._check_mkfs_fat_errors)

        mount_point = temp_dir / 'mnt'
        self._mount_fat(image_file_path, mount_point)
        try:
            self._run_tool([str(self.input_path), str(mount_point)],
                           'gen script')
        finally:
            self._umount(mount_point)
        return image_file_path

    def _run_test(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            image_path = self._create_fat_image(pathlib.Path(temp_dir))
            self._convert_to_ext4(image_path)
            self._check_ext4_image(image_path)


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
