import pathlib
import shutil
import tempfile


class SourceTree:
    def __init__(self):
        self.root = pathlib.Path(tempfile.mkdtemp())

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def remove(self):
        shutil.rmtree(self.root)
