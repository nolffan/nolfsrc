"""Companion interface; package parsing and profile publication stay in the core."""
import json,os
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from nolf_mod_core import inspect_package,sha,MANAGER_VERSION

def enable_drop(window,callback):
    """Windows Explorer file drops without a bundled third-party Tk extension."""
    if os.name!='nt':return None
    import ctypes
    from ctypes import wintypes
    window.update_idletasks()
    user=ctypes.WinDLL('user32',use_last_error=True);shell=ctypes.WinDLL('shell32',use_last_error=True)
    pointer=ctypes.c_ssize_t
    proc_type=ctypes.WINFUNCTYPE(pointer,wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM)
    setter=user.SetWindowLongPtrW if ctypes.sizeof(ctypes.c_void_p)==8 else user.SetWindowLongW
    setter.argtypes=[wintypes.HWND,ctypes.c_int,pointer];setter.restype=pointer
    user.GetParent.argtypes=[wintypes.HWND];user.GetParent.restype=wintypes.HWND
    user.CallWindowProcW.argtypes=[ctypes.c_void_p,wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM];user.CallWindowProcW.restype=pointer
    shell.DragAcceptFiles.argtypes=[wintypes.HWND,wintypes.BOOL]
    shell.DragQueryFileW.argtypes=[wintypes.HANDLE,wintypes.UINT,wintypes.LPWSTR,wintypes.UINT];shell.DragQueryFileW.restype=wintypes.UINT
    shell.DragFinish.argtypes=[wintypes.HANDLE]
    hwnd=user.GetParent(window.winfo_id()) or window.winfo_id()
    old=0
    def dispatch(h,msg,w,l):
        if msg==0x233:
            paths=[]
            try:
                count=shell.DragQueryFileW(w,0xffffffff,None,0)
                if count==1:
                    size=shell.DragQueryFileW(w,0,None,0)
                    if size<32768:
                        buf=ctypes.create_unicode_buffer(size+1);shell.DragQueryFileW(w,0,buf,size+1);paths=[buf.value]
            finally:shell.DragFinish(w)
            window.after(0,lambda:callback(paths[0] if paths else None));return 0
        return user.CallWindowProcW(old,h,msg,w,l)
    procedure=proc_type(dispatch)
    old=setter(hwnd,-4,ctypes.cast(procedure,ctypes.c_void_p).value)
    if not old:return None
    shell.DragAcceptFiles(hwnd,True)
    def close():
        shell.DragAcceptFiles(hwnd,False);setter(hwnd,-4,old)
    close.procedure=procedure
    return close


def gui(manager):
    import tkinter as tk
    from tkinter import ttk,filedialog,messagebox,simpledialog
    win=tk.Tk();win.title('NOLF Mod Manager - Profiles & Packages');win.geometry('1100x760');win.minsize(960,660)
    bg='#132e32';gold='#f2c45b';paper='#edf0e3';muted='#b5cbc5'
    win.configure(bg=bg);style=ttk.Style();style.theme_use('clam')
    style.configure('TButton',font=('Segoe UI',10),padding=(10,7))
    style.configure('Treeview',font=('Segoe UI',10),rowheight=29,background=paper,fieldbackground=paper,foreground='#143d36')
    style.configure('Treeview.Heading',font=('Segoe UI',10,'bold'))
    style.map('Treeview',background=[('selected','#23665b')],foreground=[('selected','white')])
    style.configure('TCombobox',padding=5)
    tk.Label(win,text='NO ONE LIVES FOREVER',font=('Segoe UI',23,'bold'),fg=gold,bg=bg,anchor='w').pack(fill='x',padx=24,pady=(17,0))
    tk.Label(win,text='MODS & PROFILES   /   Choose your next mission',font=('Segoe UI',11),fg=paper,bg=bg,anchor='w').pack(fill='x',padx=24,pady=(0,16))
    profile_row=tk.Frame(win,bg=bg);profile_row.pack(fill='x',padx=24)
    tk.Label(profile_row,text='Profile',fg=gold,bg=bg,font=('Segoe UI',11,'bold')).pack(side='left',padx=(0,10))
    profile_name=tk.StringVar();profiles_box=ttk.Combobox(profile_row,textvariable=profile_name,state='readonly',width=27);profiles_box.pack(side='left')
    busy=False;buttons=[];executor=ThreadPoolExecutor(max_workers=1);enabled=manager.requested();profile_keys=[]
    status=tk.StringVar();summary=tk.StringVar();detail=tk.StringVar(value='Select a mod to read its description and requirements.')
    filter_value=tk.StringVar(value='All mods');search=tk.StringVar()
    def fail(exc):messagebox.showerror('NOLF Mod Manager',str(exc),parent=win)
    def selected_profile():
        index=profiles_box.current()
        return profile_keys[index] if 0<=index<len(profile_keys) else None
    def refresh_profiles(key=None):
        nonlocal profile_keys
        profiles=manager.configurations();profile_keys=list(profiles)
        profiles_box['values']=[p['name'] for p in profiles.values()]
        chosen=key or manager.selection()['configuration']
        if chosen in profile_keys:profiles_box.current(profile_keys.index(chosen))
        else:profiles_box.set('Custom (unsaved selection)')
    def check():
        try:
            order=manager.validate_selection(enabled)
            status.set('Ready to apply. Changes take effect after restarting NOLF.')
            return order
        except (ValueError,OSError) as exc:status.set(str(exc).split('\n')[0]+'  Select Review conflicts for details.');return None
    def refresh(selected=None):
        for row in tree.get_children():tree.delete(row)
        active_path=manager.root/'active.json';active=json.loads(active_path.read_text()) if active_path.exists() else {}
        active_ids=set(active.get('enabled',[]));query=search.get().lower().strip()
        try:order=manager.resolve(enabled)
        except ValueError:order=sorted(enabled)
        entries=sorted(manager.catalog.items(),key=lambda pair:(not pair[1].get('builtin'),pair[1]['name'].lower()))
        for i,m in entries:
            kind='Built-in' if m.get('builtin') else 'Data mod'
            if filter_value.get()=='Built-in features' and not m.get('builtin'):continue
            if filter_value.get()=='Imported data mods' and m.get('builtin'):continue
            if filter_value.get()=='Enabled only' and i not in enabled:continue
            if query and query not in ' '.join(str(m.get(k,'')) for k in ('name','id','author','description','category')).lower():continue
            tree.insert('','end',iid=i,values=('ON' if i in enabled else 'OFF','ON' if i in active_ids else 'OFF',m['name'],m.get('version','local'),kind,str(order.index(i)+1) if i in order else '-'))
        if selected and tree.exists(selected):tree.selection_set(selected);tree.see(selected)
        changed=order!=active.get('enabled',[])
        summary.set('%d enabled  |  %d installed  |  %s'%(len(enabled),len(manager.catalog),'Pending changes - restart required' if changed else 'Selection matches active game profile'))
        check()
    def work(operation,finished,message):
        nonlocal busy
        if busy:return
        busy=True;status.set(message)
        for button in buttons:button.state(['disabled'])
        profiles_box.configure(state='disabled');future=executor.submit(operation)
        def poll():
            nonlocal busy
            if not future.done():win.after(100,poll);return
            busy=False
            for button in buttons:button.state(['!disabled'])
            profiles_box.configure(state='readonly')
            try:finished(future.result())
            except (ValueError,OSError,KeyError) as exc:status.set(str(exc).split('\n')[0]);fail(exc)
        win.after(100,poll)
    def choose(event=None):
        nonlocal enabled
        if busy:return
        try:
            key=selected_profile()
            if key:manager.choose_configuration(key);enabled=manager.requested();refresh()
        except (ValueError,OSError) as exc:fail(exc);refresh_profiles()
    def save_profile():
        if busy:return
        key=selected_profile()
        if key:
            try:manager.save_configuration(manager.configurations()[key]['name'],key);refresh_profiles(key);status.set('Profile saved with exact mod versions and load order.')
            except (ValueError,OSError) as exc:fail(exc)
        else:new_profile()
    def new_profile():
        if busy:return
        name=simpledialog.askstring('Save profile','Name this configuration:',parent=win)
        if name:
            try:key=manager.save_configuration(name);refresh_profiles(key);status.set('Profile saved. Campaign saves remain with this mod combination.')
            except (ValueError,OSError) as exc:fail(exc)
    def profile_action(action):
        if busy:return
        key=selected_profile()
        try:
            if action=='Import':
                path=filedialog.askopenfilename(parent=win,title='Import profile manifest',filetypes=[('Profile manifests','*.json')])
                if path:key=manager.import_configuration(path);refresh_profiles(key);status.set('Profile imported. Select it to load its choices; overrides require your approval.')
            elif not key:fail('Select a saved profile first.')
            elif action=='Export':
                path=filedialog.asksaveasfilename(parent=win,title='Export profile manifest',defaultextension='.json',filetypes=[('Profile manifests','*.json')])
                if path:manager.export_configuration(key,path);status.set('Exported profile metadata only. No game assets or saves included.')
            elif action=='Delete':
                if messagebox.askyesno('Delete configuration','Delete this named configuration? Its saved games will be retained.',parent=win):manager.delete_configuration(key);refresh_profiles();refresh()
            else:
                name=simpledialog.askstring(action+' profile','Profile name:',initialvalue=manager.configurations()[key]['name']+(' copy' if action=='Duplicate' else ''),parent=win)
                if name:
                    if action=='Duplicate':key=manager.duplicate_configuration(key,name)
                    else:manager.rename_configuration(key,name)
                    refresh_profiles(key)
        except (ValueError,OSError) as exc:fail(exc)
    for label,fn in [('Save profile',save_profile),('Save as...',new_profile)]:
        b=ttk.Button(profile_row,text=label,command=fn);b.pack(side='left',padx=(8,0));buttons.append(b)
    more=ttk.Menubutton(profile_row,text='Profile actions');menu=tk.Menu(more,tearoff=False)
    for action in ('Duplicate','Rename','Delete','Export','Import'):menu.add_command(label=action,command=lambda a=action:profile_action(a))
    more['menu']=menu;more.pack(side='left',padx=(8,0));buttons.append(more)
    profiles_box.bind('<<ComboboxSelected>>',choose)
    tk.Label(win,textvariable=summary,fg=muted,bg=bg,font=('Segoe UI',10),anchor='w').pack(fill='x',padx=24,pady=(10,12))
    filters=tk.Frame(win,bg=bg);filters.pack(fill='x',padx=24,pady=(0,10))
    ttk.Entry(filters,textvariable=search,width=35).pack(side='left',fill='x',expand=True)
    tk.Label(filters,text='  Search name, author or category  ',fg=muted,bg=bg,font=('Segoe UI',9)).pack(side='left')
    ttk.Combobox(filters,textvariable=filter_value,state='readonly',values=['All mods','Built-in features','Imported data mods','Enabled only'],width=21).pack(side='right')
    body=ttk.Frame(win);body.pack(fill='both',expand=True,padx=24)
    tree=ttk.Treeview(body,columns=('pending','active','name','version','kind','order'),show='headings',selectmode='browse')
    for key,label,width in [('pending','Pending',65),('active','Active',65),('name','Mod',350),('version','Version',85),('kind','Type',100),('order','Order',55)]:tree.heading(key,text=label);tree.column(key,width=width,stretch=key=='name',anchor='w' if key=='name' else 'center')
    scroll=ttk.Scrollbar(body,orient='vertical',command=tree.yview);tree.configure(yscrollcommand=scroll.set);tree.pack(side='left',fill='both',expand=True);scroll.pack(side='right',fill='y')
    tk.Label(win,textvariable=detail,fg=paper,bg=bg,font=('Segoe UI',10),justify='left',anchor='nw',wraplength=1020,height=4).pack(fill='x',padx=24,pady=(10,0))
    def select(event=None):
        chosen=tree.selection()
        if not chosen:return
        i=chosen[0];m=manager.catalog[i];kind='Built-in gameplay feature' if m.get('builtin') else 'Imported data-only resources'
        detail.set(m['name']+'  /  '+m.get('version','local')+'  /  '+kind+'\n'+m.get('description','No description supplied.')+'\n'+('By '+m['author']+'   ' if m.get('author') else '')+'Requires: '+(', '.join(m.get('requires',[])) or 'None')+'   |   '+m.get('category','General'))
    def toggle(event=None):
        nonlocal enabled
        if busy or not tree.selection():return
        i=tree.selection()[0]
        try:enabled=manager.toggle(enabled,i,i not in enabled);manager.save(enabled);refresh(i);select()
        except (ValueError,OSError) as exc:fail(exc)
    def move(direction):
        if busy or not tree.selection():return
        i=tree.selection()[0]
        try:
            order=manager.resolve(enabled)
            if i not in order:return
            index=order.index(i);other=index+direction
            if not 0<=other<len(order):return
            order[index],order[other]=order[other],order[index];manager.save(enabled,order=order);refresh(i)
            if manager.resolve(enabled)!=order:status.set('Dependencies and declared overrides require this ordering.')
        except (ValueError,OSError) as exc:fail(exc)
    def review():
        if busy:return
        try:
            conflicts=manager.conflicts(enabled)
            if not conflicts:manager.validate_selection(enabled);messagebox.showinfo('Configuration ready','Dependencies, versions and resource replacements are valid.',parent=win);return
            text='\n'.join(c['path']+'\n  '+c['loser']+' -> '+c['winner']+(' [declared]' if c['declared'] else ' [not declared]') for c in conflicts)
            if any(not c['declared'] for c in conflicts):messagebox.showerror('Conflicting resources',text+'\n\nDisable a conflicting mod or obtain a package that explicitly declares the override.',parent=win)
            elif messagebox.askyesno('Approve declared overrides',text+'\n\nApprove these replacements for the selected package contents?',parent=win):manager.approve_overrides([{k:v for k,v in c.items() if k!='declared'} for c in conflicts]);refresh()
        except (ValueError,OSError) as exc:fail(exc)
    def preview(path=None):
        if busy:return
        if path is None:path=filedialog.askopenfilename(parent=win,title='Preview a NOLF data package',filetypes=[('NOLF data mods','*.zip *.rez')])
        if not path:return
        if Path(path).suffix.lower() not in ('.zip','.rez'):fail('Drop or choose one ZIP or data-only REZ package.');return
        def inspect():
            digest=sha(path);meta,payload=inspect_package(path)
            if sha(path)!=digest:raise ValueError('Archive changed during preview; select it again.')
            return meta,{k:len(v) for k,v in payload.items()},digest
        def show(result):
            meta,files,digest=result;dialog=tk.Toplevel(win);dialog.title('Import preview - '+meta['name']);dialog.geometry('760x580');dialog.transient(win);dialog.grab_set();dialog.configure(bg=bg)
            tk.Label(dialog,text=meta['name'],font=('Segoe UI',19,'bold'),fg=gold,bg=bg,anchor='w',justify='left',wraplength=715).pack(fill='x',padx=20,pady=(16,4))
            text='Version '+meta['version']+'  |  '+meta.get('author','Author not supplied')+'\n'+meta.get('description','No description supplied.')
            text+='\nData-only package  |  %d resources  |  %.2f MiB'%(len(files),sum(files.values())/1024**2)
            text+='\nRequires: '+(', '.join(meta.get('requires',[])) or 'None')+'  |  Manager '+meta.get('manager_version','any')+'  |  Game '+meta.get('game_version','unspecified')
            issues=[]
            if meta['id'] in manager.catalog:issues.append('This mod ID is already installed.')
            for dep in meta.get('requires',[]):
                if dep not in manager.catalog:issues.append('Missing dependency: '+dep)
            from nolf_mod_core import resolve_mods
            catalog=dict(manager.catalog,**{meta['id']:meta});required={meta['id']};pending=[meta['id']]
            while pending:
                for dep in catalog.get(pending.pop(),{}).get('requires',[]):
                    if dep not in required:required.add(dep);pending.append(dep)
            try:resolve_mods(catalog,required,game_version=manager.info.get('game_version','unknown'))
            except ValueError as exc:issues.append(str(exc))
            text+='\n'+('Compatibility: '+'; '.join(issues) if issues else 'Package checks passed. Imports remain OFF until you enable them.')
            tk.Label(dialog,text=text,fg=paper,bg=bg,font=('Segoe UI',10),justify='left',anchor='w',wraplength=715).pack(fill='x',padx=20,pady=(0,12))
            listing=tk.Frame(dialog,bg=bg);listing.pack(fill='both',expand=True,padx=20)
            contents=ttk.Treeview(listing,columns=('path','size'),show='headings');contents.heading('path',text='Resource path');contents.heading('size',text='Bytes');contents.column('path',width=560);contents.column('size',width=90,anchor='e')
            for name,size in sorted(files.items()):contents.insert('','end',values=(name,size))
            scrollbar=ttk.Scrollbar(listing,orient='vertical',command=contents.yview);contents.configure(yscrollcommand=scrollbar.set)
            scrollbar.pack(side='right',fill='y');contents.pack(side='left',fill='both',expand=True)
            row=tk.Frame(dialog,bg=bg);row.pack(fill='x',padx=20,pady=15)
            def publish():
                dialog.destroy()
                def done(meta):refresh(meta['id']);select();status.set('Imported '+meta['name']+' - OFF. Enable it when ready.')
                work(lambda:manager.import_zip(path,expected_sha=digest),done,'Publishing validated resources...')
            ttk.Button(row,text='Cancel',command=dialog.destroy).pack(side='right')
            button=ttk.Button(row,text='Import disabled',command=publish);button.pack(side='right',padx=10)
            if meta['id'] in manager.catalog:button.state(['disabled'])
        work(inspect,show,'Inspecting ZIP and REZ contents...')
    def launch():
        if busy:return
        def operation():manager.save(enabled);return manager.launch()
        def done(child):refresh();status.set('Game launched. Pending changes apply at the next restart.');win.iconify()
        work(operation,done,'Validating and preparing your selected profile...')
    actions=tk.Frame(win,bg=bg);actions.pack(fill='x',padx=24,pady=(8,10))
    for label,fn in [('Toggle selected',toggle),('Earlier',lambda:move(-1)),('Later',lambda:move(1)),('Import ZIP/REZ...',preview),('Review conflicts',review),('Apply & launch',launch)]:
        b=ttk.Button(actions,text=label,command=fn);b.pack(side='left',padx=(0,8));buttons.append(b)
    tk.Label(win,textvariable=status,fg=gold,bg=bg,font=('Segoe UI',10),anchor='w',wraplength=1020,height=2).pack(fill='x',padx=24)
    drop=enable_drop(win,lambda path:preview(path) if path else fail('Drop one ZIP package at a time.'))
    tk.Label(win,text=('Drop a ZIP or REZ anywhere to preview it.  |  ' if drop else 'Use Import ZIP to preview a package.  |  ')+'Separate saves for each mod combination.  |  Manager '+MANAGER_VERSION,fg=muted,bg=bg,font=('Segoe UI',9),anchor='w').pack(fill='x',padx=24,pady=(0,12))
    tree.bind('<<TreeviewSelect>>',select);tree.bind('<Double-1>',toggle);tree.bind('<space>',toggle);tree.bind('<Return>',toggle)
    search.trace_add('write',lambda *args:refresh());filter_value.trace_add('write',lambda *args:refresh())
    win.bind('<Control-f>',lambda event:filters.winfo_children()[0].focus_set())
    def close():
        if busy:status.set('Finishing the current operation. Close the manager when it completes.');return
        if drop:drop()
        executor.shutdown(wait=False);win.destroy()
    def disk_state():
        paths=[manager.root/n for n in ('request.ini','active.json','configurations.json','manager.json')]
        paths+=list((manager.root/'mods').glob('*/manifest.json'))
        return tuple((str(p),p.stat().st_mtime_ns,p.stat().st_size) for p in paths if p.exists())
    observed=disk_state()
    def sync_external():
        nonlocal manager,enabled,observed
        try:
            current=disk_state()
            if not busy and not win.grab_current() and current!=observed:
                selected=tree.selection();manager=type(manager)(manager.root);enabled=manager.requested()
                refresh_profiles();refresh(selected[0] if selected else None);select();observed=current
        except (ValueError,OSError) as exc:status.set('Could not refresh saved choices: '+str(exc).split('\n')[0])
        win.after(1500,sync_external)
    win.protocol('WM_DELETE_WINDOW',close);refresh_profiles();refresh();win.after(1500,sync_external);win.mainloop()
