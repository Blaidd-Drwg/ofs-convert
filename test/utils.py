import enum
import itertools
import subprocess
import sys


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
