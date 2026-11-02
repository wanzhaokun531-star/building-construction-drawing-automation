"""Accept only a numeric measurement with the observed CAD alignment prefix."""
import re
def displayed_number(text):
 clean=re.sub(r'^\\{1,2}A1;', '', text)
 if not re.fullmatch(r'[0-9]+(?:\.[0-9]+)?',clean):raise ValueError('unrecognized dimension display syntax')
 return float(clean)
