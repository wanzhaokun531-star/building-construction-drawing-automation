#pragma once
// Actual SDK object controls only. No source drawable is sent to a renderer.
void ownershipControls(Session& s,Overrule& rule) {
    Open<AcDbBlockTableRecord> foreign(acdbSymUtil()->blockModelSpaceId(s.source.get()));
    Open<AcDbBlockTableRecord> own(acdbSymUtil()->blockModelSpaceId(s.target));
    auto record=[&](const char* name,bool passed,AcDbBlockTableRecord& value){
        s.result.ownershipChecks.emplace_back(name,passed);
        s.write(object({{"kind",q("persistentDrawableControl")},{"name",q(name)},{"passed",passed?"true":"false"},
            {"class",quote(std::wstring(value.isA()->name()))},{"handle",quote(handle(value.objectId()))},
            {"persistent",value.isPersistent()?"true":"false"},{"entityCast",AcDbEntity::cast(&value)?"true":"false"},
            {"objectDatabaseMatchesTarget",value.database()==s.target?"true":"false"},
            {"objectDatabaseMatchesSource",value.database()==s.source.get()?"true":"false"},
            {"objectIdDatabaseMatchesObject",value.objectId().database()==value.database()?"true":"false"},
            {"sourceRendererInvoked","false"}}));
    };
    record("persistent-nonentity-fixtures-real",foreign->isPersistent()&&own->isPersistent()&&
        !AcDbEntity::cast(&*foreign)&&!AcDbEntity::cast(&*own)&&foreign->database()==s.source.get()&&own->database()==s.target&&
        foreign->objectId().database()==s.source.get()&&own->objectId().database()==s.target,*foreign);
    bool ownAllowed=false,foreignRejected=false;
    try{validateChildDrawable(s,&*own);ownAllowed=true;}catch(const std::runtime_error&){}
    try{validateChildDrawable(s,&*foreign);}catch(const std::runtime_error&){foreignRejected=true;}
    record("owned-persistent-nonentity-child-allowed",ownAllowed,*own);
    record("foreign-persistent-nonentity-child-rejected",foreignRejected,*foreign);
    // Simulate the authorization window used by geometry.draw in the legacy implementation.
    // Neither drawable is rendered here. Persisted identity must dominate pointer authorization.
    const auto depth=s.authorizedTransient.size();
    struct Restore {Session& state;std::size_t depth;~Restore(){state.authorizedTransient.resize(depth);}} restore{s,depth};
    s.authorizedTransient.push_back(&*own);s.authorizedTransient.push_back(&*foreign);
    record("owned-persistent-nonentity-filter-allowed",rule.isApplicable(&*own),*own);
    record("foreign-persistent-nonentity-filter-rejected",!rule.isApplicable(&*foreign),*foreign);
    bool ownPath=false,foreignScopeRejected=false;
    try{Overrule::Scope scope(s,&*own,false);ownPath=!s.path.empty()&&s.path.back()==quote(handle(own->objectId()));}catch(const std::runtime_error&){}
    try{Overrule::Scope scope(s,&*foreign,false);}catch(const std::runtime_error&){foreignScopeRejected=true;}
    record("owned-persistent-nonentity-scope-handle-path",ownPath,*own);
    record("foreign-persistent-nonentity-scope-rejected",foreignScopeRejected,*foreign);
}
