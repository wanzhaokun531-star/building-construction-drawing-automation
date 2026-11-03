void btLabels()noexcept{try{const auto config=iniPath();isolated(ini(config,L"nonce"));const fs::path job=ini(config,L"receiptDirectory");auto& db=*curDoc()->database();Working w(&db);productDependencies();require(sha(job/L"control.dwg")==ascii(ini(config,L"sourceHash")),"label exact input");
 std::set<std::string> changed;const auto move=[&](const char* h,double y){wholeTranslate(find(db,h),{0,y,0});changed.insert(h);};
 for(auto h:{"3FBD","3FBE","3FCC","3FCD","3FE2","3FE3"})move(h,-700);
 for(auto h:{"3FBB","3FBC"})move(h,-220);
 for(auto h:{"3FC3","3FC4","3FD6","3FD7","3FE4","3FE5"})move(h,180);
 for(auto h:{"1F48","1F49","1F4A","1F4B","2E86","2E87","2E88","2E89","3968","3969","396A","396B"})move(h,180);
 approvedResultView(db);const auto file=job/L"百塘村邹总_三层平面_标注深化确认稿_v1.dwg";status(db.saveAs(file.c_str(),false,AcDb::kDHL_CURRENT),"save final label layout");std::ostringstream o;o<<"{\"rootCount\":"<<modelTypes(db).size()<<",\"sourceSHA256\":\""<<ascii(ini(config,L"sourceHash"))<<"\",\"outputSHA256\":\""<<sha(file)<<"\",\"movedRoots\":"<<changed.size()<<"}";writeNew(job/L"tidy.done",o.str());
}catch(const std::exception& e){writeNew(fs::path(ini(iniPath(),L"receiptDirectory"))/L"tidy.error",e.what());}}
