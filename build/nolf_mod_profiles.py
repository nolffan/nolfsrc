"""Named configurations; names never own or delete save directories."""
import json,re,uuid
from pathlib import Path
from nolf_mod_core import atomic_text,error,fingerprint,unique_json

class Configurations:
    def selection(self):
        from nolf_mod_manager import read_ini
        c=read_ini(self.root/'request.ini')
        try:
            result=dict(configuration=c.get('Selection','Configuration',fallback=''),
                order=json.loads(c.get('Selection','Order',fallback='[]')),
                approvals=json.loads(c.get('Selection','Approvals',fallback='[]')),
                pins=json.loads(c.get('Selection','Pins',fallback='{}')))
            if (not isinstance(result['order'],list) or len(result['order'])>64
                or any(not isinstance(i,str) for i in result['order'])
                or not isinstance(result['pins'],dict) or len(result['pins'])>64
                or not isinstance(result['approvals'],list) or len(result['approvals'])>4096
                or any(not isinstance(a,dict) or set(a)!={'path','winner','loser','winner_hash','loser_hash'}
                       or any(not isinstance(v,str) for v in a.values()) for a in result['approvals'])):
                raise ValueError('Invalid selection types')
            return result
        except (ValueError,TypeError):error('Invalid pending configuration. Select a saved profile in the manager.')

    def snapshot(self,enabled=None):
        enabled=self.requested() if enabled is None else set(enabled)
        selection=self.selection();order=self.resolve(enabled)
        return dict(schema=1,enabled=order,order=order,
            mods={i:dict(version=self.catalog[i].get('version','local'),hash=fingerprint(self.catalog[i])) for i in order},
            approvals=[a for a in selection['approvals'] if a.get('winner') in enabled and (a.get('loser')=='core' or a.get('loser') in enabled)])

    def check_configuration(self,data):
        if not isinstance(data,dict) or set(data)-{'schema','name','enabled','order','mods','approvals'} or data.get('schema')!=1:error('Invalid profile manifest')
        enabled=data.get('enabled');order=data.get('order');pins=data.get('mods');approvals=data.get('approvals',[])
        if not isinstance(enabled,list) or len(enabled)>64 or any(not isinstance(i,str) for i in enabled) or len(set(enabled))!=len(enabled):error('Invalid profile mod list')
        if not isinstance(order,list) or any(not isinstance(i,str) for i in order) or len(order)!=len(enabled) or set(order)!=set(enabled):error('Invalid profile load order')
        if not isinstance(pins,dict) or set(pins)!=set(enabled):error('Profile must pin every selected mod')
        for i,pin in pins.items():
            if i not in self.catalog:error('Profile requires missing mod: '+i)
            if pin!=dict(version=self.catalog[i].get('version','local'),hash=fingerprint(self.catalog[i])):error('Profile requires a different version or content of '+i)
        if not isinstance(approvals,list) or len(approvals)>4096 or any(not isinstance(a,dict) or set(a)!={'path','winner','loser','winner_hash','loser_hash'} or any(not isinstance(v,str) for v in a.values()) for a in approvals):error('Invalid profile override approvals')
        from nolf_mod_core import resolve_mods
        resolved=resolve_mods(self.catalog,enabled,order,self.info.get('game_version','unknown'))
        if resolved!=order:error('Profile load order violates dependency or override ordering')
        return data

    def configurations(self):
        path=self.root/'configurations.json'
        if path.exists():
            data=unique_json(path.read_text(encoding='utf-8'))
            if not isinstance(data,dict) or data.get('schema')!=1 or not isinstance(data.get('profiles'),dict):error('Invalid configuration library')
            return data['profiles']
        # Read-only defaults until the first explicit save/rename/select.
        enhanced=self.requested()
        try:self.resolve(enhanced)
        except ValueError:enhanced={i for i,m in self.catalog.items() if m.get('builtin')}
        return {key:dict(name=name,**self.snapshot(enabled)) for key,name,enabled in
                [('classic','Classic NOLF',set()),('enhanced','Enhanced NOLF',enhanced),('custom','Custom',enhanced)]}

    def configuration_name(self):
        key=self.selection()['configuration']
        profiles=self.configurations()
        if key not in profiles:return 'Custom'
        profile=profiles[key]
        try:current=self.snapshot()
        except ValueError:return profile['name']+' (invalid pending choices)'
        changed=any(current.get(k)!=profile.get(k) for k in ('enabled','order','mods','approvals'))
        return profile['name']+(' (modified)' if changed else '')

    def write_configurations(self,profiles):
        atomic_text(self.root/'configurations.json',json.dumps(dict(schema=1,profiles=profiles),indent=2))

    @staticmethod
    def valid_name(name):
        if not isinstance(name,str) or not name.strip() or len(name)>60 or any(ord(c)<32 or ord(c)>126 for c in name):error('Use a profile name of 1-60 printable characters')
        return name.strip()

    def save_configuration(self,name,key=None):
        name=self.valid_name(name)
        with self.lock():
            profiles=self.configurations()
            if key and key not in profiles:error('Unknown profile')
            if any(p['name'].casefold()==name.casefold() for k,p in profiles.items() if k!=key):error('A profile with this name already exists')
            if len(profiles)>=64 and not key:error('Profile limit reached')
            key=key or 'custom-'+uuid.uuid4().hex[:12]
            profiles[key]=dict(name=name,**self.snapshot());self.write_configurations(profiles)
            data=profiles[key];self.save(data['enabled'],order=data['order'],approvals=data['approvals'],configuration=key,pins=data['mods'])
        return key

    def choose_configuration(self,key):
        with self.lock():
            profiles=self.configurations()
            if key not in profiles:error('Unknown profile')
            data=self.check_configuration(profiles[key])
            self.write_configurations(profiles)
            self.save(data['enabled'],order=data['order'],approvals=data['approvals'],configuration=key,pins=data['mods'])

    def rename_configuration(self,key,name):
        name=self.valid_name(name)
        with self.lock():
            profiles=self.configurations()
            if key not in profiles:error('Unknown profile')
            if any(p['name'].casefold()==name.casefold() for k,p in profiles.items() if k!=key):error('A profile with this name already exists')
            profiles[key]['name']=name;self.write_configurations(profiles)

    def duplicate_configuration(self,key,name):
        name=self.valid_name(name)
        with self.lock():
            profiles=self.configurations()
            if key not in profiles:error('Unknown profile')
            if len(profiles)>=64:error('Profile limit reached')
            if any(p['name'].casefold()==name.casefold() for p in profiles.values()):error('A profile with this name already exists')
            new='custom-'+uuid.uuid4().hex[:12];profiles[new]=dict(profiles[key],name=name);self.write_configurations(profiles)
        return new

    def delete_configuration(self,key):
        with self.lock():
            profiles=self.configurations()
            if key not in profiles:error('Unknown profile')
            del profiles[key];self.write_configurations(profiles)
        # Deliberately retains all immutable combinations and their saves.

    def export_configuration(self,key,path):
        profiles=self.configurations()
        if key not in profiles:error('Unknown profile')
        atomic_text(path,json.dumps(profiles[key],indent=2))

    def import_configuration(self,path):
        path=Path(path)
        if path.stat().st_size>1024*1024:error('Profile manifest is too large')
        data=self.check_configuration(unique_json(path.read_text(encoding='utf-8')))
        # Imported declarations cannot grant this player's override approval.
        data=dict(data,approvals=[],name=self.valid_name(data.get('name','Imported profile')))
        with self.lock():
            profiles=self.configurations()
            if len(profiles)>=64:error('Profile limit reached')
            name=data['name'];suffix=2
            while any(p['name'].casefold()==data['name'].casefold() for p in profiles.values()):data['name']=name+' '+str(suffix);suffix+=1
            key='custom-'+uuid.uuid4().hex[:12];profiles[key]=data;self.write_configurations(profiles)
        return key
