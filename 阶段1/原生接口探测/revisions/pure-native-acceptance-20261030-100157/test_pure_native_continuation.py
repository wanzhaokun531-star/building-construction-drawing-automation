import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from verify_pure_native_continuation import exact_font_loaded,complete_native_catalog,inspect
class PureNativeAcceptance(unittest.TestCase):
 def saved_text(self):
  originals=[dict(handle=str(n),type='TDbWall') for n in range(213)]
  return dict(contract='stage1.native-generic-text-copy.v1',error=None,COMUsed=False,ComputerUseUsed=False,TianzhengBusinessCommandsUsed=False,commandTrace=dict(lost=False,commands=[]),sourceCount=213,originalBefore=originals,originalAfter=originals,originalSaved=originals,rootType='TDbText',savedRootType='TDbText',savedModelCount=214,standardControlStatus=0,copyFromStatus=0,saveStatus=0,businessContentWritePassed=True)
 def test_saved_valid_native_text_fixture(self):self.assertTrue(all(x for x,_ in inspect(self.saved_text())))
 def test_saved_root_replacement_is_rejected(self):
  row=self.saved_text();row['savedRootType']='AcDbText';self.assertFalse(all(x for x,_ in inspect(row)))
 def test_saved_original_change_is_rejected(self):
  row=self.saved_text();row['originalSaved']=[];self.assertFalse(all(x for x,_ in inspect(row)))
 def test_saved_wrong_model_count_is_rejected(self):
  row=self.saved_text();row['savedModelCount']=215;self.assertFalse(all(x for x,_ in inspect(row)))
 def font(self):return dict(loadBits=1,conversionStatus=0,loadedPrimary='gbenor.shx',declaredPrimary='gbenor.shx',loadedBig='gbcbig.shx',declaredBig='gbcbig.shx',resolvedFiles=[dict(status=0,fileExists=True,path='D:/CAD/Fonts/gbenor.shx'),dict(status=0,fileExists=True,path='D:/CAD/Fonts/gbcbig.shx')])
 def catalog(self):return dict(expectedCount=2,initialModelCount=2,finalModelCount=2,originalBefore=[dict(handle='1',type='TDbWall'),dict(handle='2',type='TDbText')],originalAfter=[dict(handle='1',type='TDbWall'),dict(handle='2',type='TDbText')],samples=[dict(handle='1',actualType='TDbWall',returned=True,error=None),dict(handle='2',actualType='TDbText',returned=True,error=None)],error=None)
 def test_exact_loaded_fonts_are_accepted(self):self.assertTrue(exact_font_loaded(self.font()))
 def test_substitution_is_not_exact_font_pass(self):
  row=self.font();row['loadBits']=9;self.assertFalse(exact_font_loaded(row))
 def test_bigfont_failure_is_not_exact_font_pass(self):
  row=self.font();row['loadBits']=5;self.assertFalse(exact_font_loaded(row))
 def test_loaded_font_filename_change_is_not_exact(self):
  row=self.font();row['loadedPrimary']='txt.shx';self.assertFalse(exact_font_loaded(row))
 def test_unresolved_font_file_is_not_exact(self):
  row=self.font();row['resolvedFiles'][0]['fileExists']=False;self.assertFalse(exact_font_loaded(row))
 def test_ttf_face_cannot_be_proved_by_shx_filename(self):
  row=self.font();row['before']=dict(font=dict(typeface='宋体'));self.assertFalse(exact_font_loaded(row))
 def test_extension_completion_requires_same_physical_files(self):
  row=self.font();row['loadBits']=25;row['beforeResolvedFiles']=copy.deepcopy(row['resolvedFiles'])
  for files in (row['resolvedFiles'],row['beforeResolvedFiles']):
   for n,item in enumerate(files):item['fileIdentity']=dict(volume=4,indexHigh=0,indexLow=n+1,bytes=100,writeHigh=1,writeLow=2)
  self.assertTrue(exact_font_loaded(row))
 def test_equal_names_but_different_physical_files_are_rejected(self):
  row=self.font();row['loadBits']=25;row['beforeResolvedFiles']=copy.deepcopy(row['resolvedFiles'])
  for n,item in enumerate(row['resolvedFiles']):item['fileIdentity']=dict(volume=4,indexHigh=0,indexLow=n+1,bytes=100,writeHigh=1,writeLow=2)
  row['beforeResolvedFiles']=copy.deepcopy(row['resolvedFiles']);row['beforeResolvedFiles'][0]['fileIdentity']['indexLow']=99
  self.assertFalse(exact_font_loaded(row))
 def test_complete_actual_entity_set_is_accepted(self):self.assertTrue(complete_native_catalog(self.catalog()))
 def test_missing_native_sample_is_rejected(self):
  row=self.catalog();row['samples'].pop();self.assertFalse(complete_native_catalog(row))
 def test_duplicate_native_sample_is_rejected(self):
  row=self.catalog();row['samples'].append(copy.deepcopy(row['samples'][0]));self.assertFalse(complete_native_catalog(row))
 def test_changed_original_is_rejected(self):
  row=self.catalog();row['originalAfter'][0]['type']='AcDbLine';self.assertFalse(complete_native_catalog(row))
 def test_wrong_native_sample_type_is_rejected(self):
  row=self.catalog();row['samples'][0]['actualType']='AcDbLine';self.assertFalse(complete_native_catalog(row))
if __name__=='__main__':unittest.main(verbosity=2)
