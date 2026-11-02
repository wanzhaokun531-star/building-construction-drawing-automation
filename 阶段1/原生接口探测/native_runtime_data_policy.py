from pathlib import PureWindowsPath
def data_names_allowed(names):return bool(names) and all(n.casefold()=='linetype.dbf' and PureWindowsPath(n).name==n for n in names)
def add_data_support_path(existing,directory):
 parts=[x for x in existing.split(';') if x]
 if str(PureWindowsPath(directory)).casefold() in {str(PureWindowsPath(x)).casefold() for x in parts}:return existing
 return existing+('' if not existing or existing.endswith(';') else ';')+directory+';'
