#!/usr/bin/env python3
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


CONVERT_TIMEOUT = 5
FSCK_TIMEOUT = 5


class FatConversionTest(unittest.TestCase):
    pass


def write_tool_output(output, input_image_path: pathlib.Path, suffix):
    name = '{}.{}.txt'.format(input_image_path.stem, suffix)
    out_path = input_image_path.parent / name
    if out_path.exists():
        out_path.unlink()
    if output:
        out_path.write_bytes(output)


def convert(test_case, input_image_path, temp_copy_path):
    try:
        proc = subprocess.run(
            (test_case.OFS_CONVERT, str(temp_copy_path)),
            stderr=subprocess.PIPE, stdout=subprocess.PIPE,
            timeout=CONVERT_TIMEOUT)
    except subprocess.TimeoutExpired:
            test_case.fail('Conversion timed out')
    write_tool_output(proc.stderr, input_image_path, 'conv-err')
    write_tool_output(proc.stdout, input_image_path, 'conv-out')
    test_case.assertEqual(0, proc.returncode, 'Conversion did not exit cleanly')


def check(test_case, input_image_path, ext4_image_path):
    try:
        proc = subprocess.run(('fsck.ext4', '-n', '-f', str(ext4_image_path)),
                              stderr=subprocess.PIPE, stdout=subprocess.PIPE,
                              timeout=FSCK_TIMEOUT)
    except subprocess.TimeoutExpired:
            test_case.fail('fsck.ext4 timed out')
    write_tool_output(proc.stderr, input_image_path, 'fsck-err')
    write_tool_output(proc.stdout, input_image_path, 'fsck-out')
    if proc.returncode & ~12 == 0:
        err_msg = 'fsck.ext4 reported errors in image'
    else:
        err_msg = 'fsck.ext4 exited with unexpected exit code'
    test_case.assertEqual(0, proc.returncode, err_msg)


def run_test(testcase, fat_image_path):
    with tempfile.NamedTemporaryFile() as temp_file:
        with fat_image_path.open('rb') as src_file:
            shutil.copyfileobj(src_file, temp_file)
        temp_file.flush()
        temp_file_path = pathlib.Path(temp_file.name)
        convert(testcase, fat_image_path, temp_file_path)
        check(testcase, fat_image_path, temp_file_path)


def make_test_method(fat_image):
    def test(self):
        run_test(self, fat_image)
    return test


def setup_test_methods(ofs_convert_path, image_dir):
    FatConversionTest.OFS_CONVERT = ofs_convert_path
    for fat_image_path in pathlib.Path(image_dir).glob('**/*.fat'):
        test_meth = make_test_method(fat_image_path)
        setattr(FatConversionTest, 'test_' + fat_image_path.stem, test_meth)


def main():
    if len(sys.argv) != 3:
        print('Usage: {} ofs-convert image_dir'.format(sys.argv[0]))
        exit(1)

    setup_test_methods(sys.argv[1], sys.argv[2])
    unittest.main(argv=sys.argv[:1])


if __name__ == '__main__':
    # Executed as a script, load arguments from argv
    main()
else:
    # Imported as a unittest module, load from env variables
    import os

    setup_test_methods(os.environ['OFS_CONVERT'],
                       os.environ['OFS_CONVERT_IMAGE_DIR'])
