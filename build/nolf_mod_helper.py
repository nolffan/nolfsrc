"""Windowless release entry point; errors are retained even without a console."""
import json,sys,traceback
from pathlib import Path
from nolf_mod_core import atomic_text
from nolf_mod_manager import main
from nolf_mod_portable import user_home

if __name__=='__main__':
    try:main()
    except Exception as exc:
        report=dict(ok=False,error=str(exc))
        if '--result' in sys.argv:
            try:atomic_text(Path(sys.argv[sys.argv.index('--result')+1]),json.dumps(report,indent=2))
            except (OSError,IndexError):pass
        try:atomic_text(user_home()/'last-error.txt',traceback.format_exc())
        except OSError:pass
        if '--launch' in sys.argv or '--gui' in sys.argv:
            import ctypes
            ctypes.windll.user32.MessageBoxW(None,'NOLF could not prepare the selected mods.\n\n'+str(exc)+'\n\nYour previous profiles and saves have been retained.','NOLF Mods',0x10)
        sys.exit(1)
