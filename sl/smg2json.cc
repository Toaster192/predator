/*
 * Copyright (C) 2010-2022 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "symplot.hh"

#include <cl/cl_msg.hh>
#include <cl/clutil.hh>
#include <cl/storage.hh>
#include "llvm/IR/Instructions.h"
#include <llvm/Support/raw_ostream.h>

#include "plotenum.hh"
#include "symheap.hh"
#include "sympred.hh"
#include "symseg.hh"
#include "util.hh"
#include "worklist.hh"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <string>

#include "json.hpp"


using json = nlohmann::json;

// /////////////////////////////////////////////////////////////////////////////
// implementation of JsonData()
struct SMGData {
    typedef std::pair<TObjId, TOffset>                      TFieldKey;
    typedef std::map<TFieldKey, FldList>                    TLiveFields;
    typedef std::pair<int /* ID */, TValId>                 TDangVal;
    typedef std::vector<TDangVal>                           TDangValues;

    SymHeap                            &sh;
    json                               &j;
    const TObjSet                      &objs;
    const TValSet                      &values;
    int                                 last;
    TLiveFields                         liveFields;
    TFldSet                             lonelyFields;
    TDangValues                         dangVals;

    SMGData(
            const SymHeap              &sh_,
            json                       &j_,
            const TObjSet              &objs_,
            const TValSet              &values_,
            int                        last_):
        sh(const_cast<SymHeap &>(sh_)),
        j(j_),
        objs(objs_),
        values(values_),
        last(last_)
    {
    }
};

#define GEN_labelByCode(cst) case cst: return #cst

#define SL_QUOTE(what) "\"" << what << "\""

const char* getStorClassStr(const EStorageClass code)
{
    switch (code) {
        GEN_labelByCode(SC_INVALID);
        GEN_labelByCode(SC_UNKNOWN);
        GEN_labelByCode(SC_STATIC);
        GEN_labelByCode(SC_ON_HEAP);
        GEN_labelByCode(SC_ON_STACK);
    }

    CL_BREAK_IF("invalid call of getStorClassStr()");
    return "";
}

const char* getTargetSpecStr(const ETargetSpecifier code)
{
    switch (code) {
        GEN_labelByCode(TS_INVALID);
        GEN_labelByCode(TS_REGION);
        GEN_labelByCode(TS_FIRST);
        GEN_labelByCode(TS_LAST);
        GEN_labelByCode(TS_ALL);
    }

    CL_BREAK_IF("invalid call of getTargetSpecStr()");
    return "";
}

inline const char* addSign(const TOffset off)
{
    return (off < 0)
        ? ""
        : "+";
}

#define SIGNED_OFF(off) addSign(off) << (off)

void jsonifyOffset(SMGData &data, const TOffset off, const int from, const int to)
{
    data.j["edges"] += {{"label", "offset"}, {"from", from}, {"to", to}, {"off", off}};
}

class CltFinder {
    private:
        const TObjType          cltRoot_;
        const TObjType          cltToSeek_;
        const TOffset           offToSeek_;
        TFieldIdxChain          icFound_;

    public:
        CltFinder(TObjType cltRoot, TObjType cltToSeek, TOffset offToSeek):
            cltRoot_(cltRoot),
            cltToSeek_(cltToSeek),
            offToSeek_(offToSeek)
        {
        }

        const TFieldIdxChain& icFound() const { return icFound_; }

        bool operator()(const TFieldIdxChain &ic, const struct cl_type_item *it)
        {
            const TObjType clt = it->type;
            if (clt != cltToSeek_)
                return /* continue */ true;

            const TOffset off = offsetByIdxChain(cltRoot_, ic);
            if (offToSeek_ != off)
                return /* continue */ true;

            // matched!
            icFound_ = ic;
            return false;
        }
};

bool myDigIcByOffset(
        TFieldIdxChain                  *pDst,
        const TObjType                  cltRoot,
        const TObjType                  cltField,
        const TOffset                   offRoot)
{
    CL_BREAK_IF(!cltRoot || !cltField);
    if (!offRoot && (*cltRoot == *cltField))
        // the root matches --> no fields on the way
        return false;

    CltFinder visitor(cltRoot, cltField, offRoot);
    if (traverseTypeIc(cltRoot, visitor, /* digOnlyComposite */ true))
        // not found
        return false;

    *pDst = visitor.icFound();
    return true;
}

json describeVarCore(int *pInst, SMGData &data, const TObjId obj)
{
    SymHeap &sh = data.sh;
    TStorRef stor = sh.stor();
    json out;

    CallInst ci(-1, -1);
    if (sh.isAnonStackObj(obj, &ci)) {
        // anonymous stack object
        out["label"] = "var_anonymous";
        std::string name;
        name += "STACK of ";
        if (-1 == ci.uid){
            name += "FNC_INVALID";
        }else{
            name += nameOf(*stor.fncs[ci.uid]);
            name += "()";
        }
        out["value"] = name;
        *pInst = ci.inst;
    }
    else {
        // var lookup
        out["label"] = "var";
        const CVar cv = sh.cVarByObject(obj);
        json j;
        const CodeStorage::Var &var = stor.vars[cv.uid];
        j["uid"] = var.uid;
        if (!var.name.empty()){
            j["name"] = var.name;
        }
        out["var"] = j;
        json loc;
        loc["file"] = var.loc.file;
        loc["line"] = var.loc.line;
        loc["column"] = var.loc.column;
        if (var.loc.llvm_insn){
            std::string str;
            llvm::raw_string_ostream ss(str);
            ss << *((llvm::Instruction*) var.loc.llvm_insn);
            loc["insn"] = ss.str();
        }

        out["loc"] = loc;
        *pInst = cv.inst;
    }
    return out;
}

json describeVar(SMGData &data, const TObjId obj)
{
    json out;
    if (OBJ_RETURN == obj) {
        out["label"] = "OBJ_RETURN";
        return out;
    }

    int inst;
    if (data.sh.isValid(obj)){
        out = describeVarCore(&inst, data, obj);
    } else {
        out["label"] = "obj";
        inst = -1;
    }

    out["id"] = obj;
    if (1 < inst){
        out["inst"] = inst;
    }

    return out;
}

std::string describeFieldPlacement(SMGData &data, const FldHandle &fld, TObjType clt)
{
    std::string out;
    (void) data;
    const TObjType cltField = fld.type();
    if (!cltField || *cltField == *clt)
        // nothing interesting here
        return out;

    // read field offset
    const TOffset off = fld.offset();

    TFieldIdxChain ic;
    if (!myDigIcByOffset(&ic, clt, cltField, off))
        // type of the field not found in clt
        return out;

    // chain of indexes found!
    // TODO (kinst): return this as nested json instead?
    for (const int idx : ic) {
        CL_BREAK_IF(clt->item_cnt <= idx);
        const cl_type_item *item = clt->items + idx;
        const cl_type_e code = clt->code;

        if (CL_TYPE_ARRAY == code) {
            // TODO: support non-zero indexes? (not supported by CltFinder yet)
            CL_BREAK_IF(item->offset);
        out += "[0]";
        }
        else {
            // read field name
            const char *name = item->name;
            if (!name)
                name = "<anon>";

        out += ".";
        out += name;
        }

        // jump to the next item
        clt = item->type;
    }
    return out;
}

void describeField(SMGData &data, const FldHandle &fld, const bool lonely, json& j)
{
    SymHeap &sh = data.sh;
    const TObjId obj = fld.obj();
    const EStorageClass code = sh.objStorClass(obj);

    std::string tag;
    json var;
    std::string placement;

    if (lonely && isProgramVar(code)) {
        var = describeVar(data, obj);
        tag = "field";
    }

    const TObjType cltRoot = sh.objEstimatedType(obj);
    if (cltRoot)
        placement = describeFieldPlacement(data, fld, cltRoot);

    if (!tag.empty()){
        j["label"] = tag;
    } else {
        j["label"] = "empty";
    }
    if (!var.empty()){
        j["value"] = var;
    }
    if (!placement.empty()){
        j["placement"] = placement;
    }
}

std::string intToStr(const IR::TInt i)
{
    if (IR::IntMin == i)
        return std::string("-inf");
    else if (IR::IntMax == i)
        return std::string("inf");
    else
        return std::string(std::to_string(i));
}

json jsonifyRawObject(SMGData &data, const TObjId obj)
{
    SymHeap &sh = data.sh;
    const TSizeRange size = sh.objSize(obj);

    json j;

    const EStorageClass code = sh.objStorClass(obj);

    j = {{"id", obj}, {"label", getStorClassStr(code)}};

    if (isProgramVar(code)){
        j["value"] = describeVar(data, obj);
    }else{
        const cl_loc loc = sh.getObjLoc(obj);
        if(loc.file){
            json loc_j;
            loc_j["file"] = loc.file;
            loc_j["line"] = loc.line;
            loc_j["column"] = loc.column;
            if (loc.llvm_insn){
                std::string str;
                llvm::raw_string_ostream ss(str);
                ss << *((llvm::Instruction*) loc.llvm_insn);
                loc_j["insn"] = ss.str();
            }

            j["loc"] = loc_j;
        }
    }

    j["size_low"] = size.lo;
    j["size_high"] = size.hi;

    return j;
}

bool jsonifyField(SMGData &data, const FieldWrapper &fw, const bool lonely, json& j)
{
    const FldHandle &fld = fw.fld;
    CL_BREAK_IF(!fld.isValidHandle());

    const EFieldClass code = fw.code;
    if (code == FC_VOID){
        CL_BREAK_IF("jsonifyField() got an object of class FC_VOID");
        return false;
    }

    // update filed lookup
    const TObjId obj = fld.obj();
    const SMGData::TFieldKey key(obj, fld.offset());
    data.liveFields[key].push_back(fld);

    int id = fld.fieldId();

    if (lonely) {
        id = obj;
    }

    j["id"] = id;
    describeField(data, fld, lonely, j);

    if (FC_DATA == code)
        j["size"] = fld.type()->size;

    return true;
}

void jsonifyUniformBlocks(SMGData &data, const TObjId obj, json& j)
{
    SymHeap &sh = data.sh;

    // get all uniform blocks inside the given object
    TUniBlockMap bMap;
    sh.gatherUniformBlocks(bMap, obj);

    // data all uniform blocks
    for (TUniBlockMap::const_reference item : bMap) {
        const UniformBlock &bl = item.second;

        // jsonify block node
        const int id = ++data.last;
        json out = {{"id", id}, {"label", "UNIFORM_BLOCK"}, {"size", bl.size}, {"lonely", true}};
        j += out;

        // jsonify offset edge
        const TOffset off = bl.off;
        CL_BREAK_IF(off < 0);

        data.j["edges"] += {{"label", "offset"}, {"from", obj}, {"to", id}, {"off", off}};
        // schedule hasValue edge
        const SMGData::TDangVal dv(id, bl.tplValue);
        data.dangVals.push_back(dv);
    }
}

template <class TCont>
void jsonifyFields(SMGData &data, const TObjId obj, const TCont &liveFields, json& j)
{
    SymHeap &sh = data.sh;

    FldHandle next;
    FldHandle prev;

    const EObjKind kind = sh.objKind(obj);
    switch (kind) {
        case OK_REGION:
        case OK_OBJ_OR_NULL:
            break;

        case OK_DLS:
        case OK_SEE_THROUGH_2N:
            prev = prevPtrFromSeg(sh, obj);
            // fall through!

        case OK_SEE_THROUGH:
        case OK_SLS:
            next = nextPtrFromSeg(sh, obj);
    }

    // sort objects by offset
    typedef std::vector<FieldWrapper>           TAtomList;
    typedef std::map<TOffset, TAtomList>        TAtomByOff;
    TAtomByOff objByOff;
    for (const FldHandle &fld : liveFields) {
        EFieldClass code;
        if (fld == next)
            code = FC_NEXT;
        else if (fld == prev)
            code = FC_PREV;
        else if (isDataPtr(fld.type()))
            code = FC_PTR;
        else
            code = FC_DATA;

        const TOffset off = fld.offset();
        FieldWrapper fw(fld, code);
        objByOff[off].push_back(fw);
    }

    // jsonify all atomic objects inside
    for (TAtomByOff::const_reference item : objByOff) {
        const TOffset off = item.first;
        for (const FieldWrapper &fw : /* TAtomList */ item.second) {
            // jsonify a single object
            json out;
            if (!jsonifyField(data, fw, /* lonely */ false, out))
                continue;

            j += out;
            // connect the field with the object by an offset edge
            jsonifyOffset(data, off, obj, fw.fld.fieldId());
        }
    }
}

void getCompObjDataJson(const SymHeap &sh, const TObjId obj, json &out)
{
    const TProtoLevel protoLevel= sh.objProtoLevel(obj);
    if (protoLevel)
        out["protolevel"] = (int) protoLevel;

    const EObjKind kind = sh.objKind(obj);
    switch (kind) {
        case OK_REGION:
            out["label"] = "region";
            return;

        case OK_OBJ_OR_NULL:
        case OK_SEE_THROUGH:
        case OK_SEE_THROUGH_2N:
            out["label"] = "0..1";
            break;

        case OK_SLS:
            out["label"] = "SLS";
            break;

        case OK_DLS:
            out["label"] = "DLS";
            break;
    }

    switch (kind) {
        case OK_SLS:
        case OK_DLS:
            // append minimal segment length
            out["segMinLength"] = (int) sh.segMinLength(obj);

        default:
            break;
    }

    if (OK_OBJ_OR_NULL != kind) {
        const BindingOff &bf = sh.segBinding(obj);
        switch (kind) {
            case OK_SLS:
            case OK_DLS:
                out["headOffset"] = (int) bf.head;

            default:
                break;
        }

        switch (kind) {
            case OK_SEE_THROUGH:
            case OK_SLS:
            case OK_DLS:
                out["nextOffset"] = (int) bf.next;

            default:
                break;
        }

        if (OK_DLS == kind)
            out["prevOffset"] = (int) bf.prev;
    }
}

template <class TCont>
void jsonifyCompositeObj(SMGData &data, const TObjId obj, const TCont &liveFields)
{
    SymHeap &sh = data.sh;

    json j = {{"id", ++data.last}, {"objects", json::array()}};
    getCompObjDataJson(sh, obj, j);

    j["objects"] += jsonifyRawObject(data, obj);

    // jsonify all uniform blocks
    jsonifyUniformBlocks(data, obj, j["objects"]);

    // jsonify all atomic objects inside
    jsonifyFields(data, obj, liveFields, j["objects"]);

    // save cluster
    data.j["compositeObjects"] += j;
}

bool jsonifyLonelyField(SMGData &data, const FldHandle &fld)
{
    SymHeap &sh = data.sh;

    if (fld.offset())
        // offset detected
        return false;

    const TObjId obj = fld.obj();
    if (sh.pointedByCount(obj))
        // object pointed
        return false;

    // TODO: support for objects with variable size?
    const TSizeRange size = sh.objSize(obj);
    CL_BREAK_IF(!isSingular(size));

    const TObjType clt = fld.type();
    CL_BREAK_IF(!clt);
    if (clt->size != size.lo)
        // size mismatch detected
        return false;

    data.lonelyFields.insert(fld);

    const FieldWrapper fw(fld);
    json j;
    jsonifyField(data, fw, /* lonely */ true, j);
    data.j["objects"] += j;
    return true;
}

void jsonifyObjects(SMGData &data)
{
    SymHeap &sh = data.sh;

    // go through roots
    for (const TObjId obj : data.objs) {
        // gather live objects
        FldList liveFields;
        sh.gatherLiveFields(liveFields, obj);

        if (OK_REGION == sh.objKind(obj)
                && (1 == liveFields.size())
                && jsonifyLonelyField(data, liveFields.front()))
            // this one went out in a simplified form
            continue;

        jsonifyCompositeObj(data, obj, liveFields);
    }
}

const char* getOriginLabelStr(const EValueOrigin code)
{
    switch (code) {
        GEN_labelByCode(VO_INVALID);
        GEN_labelByCode(VO_ASSIGNED);
        GEN_labelByCode(VO_UNKNOWN);
        GEN_labelByCode(VO_REINTERPRET);
        GEN_labelByCode(VO_DEREF_FAILED);
        GEN_labelByCode(VO_STACK);
        GEN_labelByCode(VO_HEAP);
    }

    CL_BREAK_IF("invalid call of getOriginLabelStr()");
    return "";
}

const char* getTargetLabelStr(const EValueTarget code)
{
    switch (code) {
        GEN_labelByCode(VT_INVALID);
        GEN_labelByCode(VT_UNKNOWN);
        GEN_labelByCode(VT_COMPOSITE);
        GEN_labelByCode(VT_CUSTOM);
        GEN_labelByCode(VT_OBJECT);
        GEN_labelByCode(VT_RANGE);
    }

    CL_BREAK_IF("invalid call of getTargetLabelStr()");
    return "";
}

json describeCustomValue(SMGData &data, const TValId val)
{
    SymHeap &sh = data.sh;
    const CustomValue cVal = sh.valUnwrapCustom(val);

    json j;

    const ECustomValue code = cVal.code();
    switch (code) {
        case CV_INVALID:
            j = {{"label", "CV_INVALID"}};
            break;

        case CV_INT_RANGE: {
            const IR::Range &rng = cVal.rng();
            if (isSingular(rng))
                j = {{"label", "int"}, {"value", rng.lo}, {"iid", val}};
            else
                // todo add alignment?
                /*
                if (isAligned(rng))
                    str << ", alignment = " << rng.alignment << suffix;
                */
                j = {{"label", "int_range"}, {"value_low", rng.lo}, {"value_low", rng.hi}, {"iid", val}};
            break;
        }

        case CV_REAL:
            j = {{"label", "real"}, {"value", cVal.fpn()}, {"iid", val}};
            break;

        case CV_FNC: {
            TStorRef stor = data.sh.stor();
            const CodeStorage::Fnc *fnc = stor.fncs[cVal.uid()];
            CL_BREAK_IF(!fnc);
  
            const std::string name = nameOf(*fnc);
            j = {{"label", "fnc"}, {"value", name}, {"iid", val}};
            break;
        }

        case CV_STRING:
            j = {{"label", "str"}, {"value", cVal.str()}, {"iid", val}};
            break;
    }
    return j;
}

void jsonifyCustomValue(
        SMGData                       &data,
        const int                       idFrom,
        const TValId                    val)
{
    const int id = ++data.last;

    json val_json = describeCustomValue(data, val);
    val_json["id"] = id;
    val_json["lonely"] = true;
    data.j["values"] += val_json;

    data.j["edges"] += {{"label", "hasValue"}, {"from", idFrom}, {"to", id}};
}

void jsonifySingleValue(SMGData &data, const TValId val)
{
    SymHeap &sh = data.sh;

    const TObjId obj = sh.objByAddr(val);

    const EValueTarget code = sh.valTarget(val);
    if (code == VT_CUSTOM){
        // skip it, custom values are now handled in jsonifyHasValue()
        return;
    }

    json j;
    if (code == VT_UNKNOWN){
        j = {{"id", val},{"label", getOriginLabelStr(sh.valOrigin(val))}};
    } else{
        j = {{"id", val},{"label", getTargetLabelStr(code)}};
    }

    if (isAnyDataArea(code)) {
        const IR::Range &offRange = sh.valOffsetRange(val);
        j["offset_low"] = offRange.lo;
        j["offset_high"] = offRange.hi;

        const ETargetSpecifier ts = sh.targetSpec(val);
        if (TS_REGION != ts)
            j["targetSpecLabel"] = getTargetSpecStr(ts);

        j["obj"] = obj;

        const cl_loc loc = sh.getObjLoc(obj);
        if(loc.file){
            json loc_j;
            loc_j["file"] = loc.file;
            loc_j["line"] = loc.line;
            loc_j["column"] = loc.column;
            if (loc.llvm_insn){
                std::string str;
                llvm::raw_string_ostream ss(str);
                ss << *((llvm::Instruction*) loc.llvm_insn);
                loc_j["insn"] = ss.str();
            }

            j["loc"] = loc_j;
        }
    }

    data.j["values"] += j;
}

void jsonifyAddrs(SMGData &data)
{
    SymHeap &sh = data.sh;

    for (const TValId val : data.values) {
        // jsonify a value node
        jsonifySingleValue(data, val);

        const TObjId obj = sh.objByAddr(val);

        const EValueTarget code = sh.valTarget(val);
        switch (code) {
            case VT_OBJECT:
                break;

            case VT_RANGE:
                data.j["edges"] += {{"label", "range"}, {"from", val}, {"to", obj}};
                continue;

            default:
                continue;
        }

        const TOffset off = sh.valOffset(val);
        if (off) {
            const SMGData::TFieldKey key(obj, off);
            SMGData::TLiveFields::const_iterator it = data.liveFields.find(key);
            if ((data.liveFields.end() != it) && (1 == it->second.size())) {
                // jsonify the target field as an abbreviation
                const FldHandle &target = it->second.front();
                data.j["edges"] += {{"label", "pointsTo"}, {"from", val}, {"to", target.fieldId()}};
                continue;
            }
        }

        jsonifyOffset(data, off, val, obj);
    }

    // go through value prototypes used in uniform blocks
    for (SMGData::TDangValues::const_reference item : data.dangVals) {
        const TValId val = item.second;
        if (val <= 0)
            continue;

        // jsonify a value node
        CL_BREAK_IF(isAnyDataArea(sh.valTarget(val)));
        jsonifySingleValue(data, val);
    }
}

/* TODO (kinst)
const char* getNullLabelStr(const SymHeapCore &sh, const TFldId fld)
{
    const FldHandle hdl(const_cast<SymHeapCore &>(sh), fld);
    const TObjType clt = hdl.type();
    if (!clt)
        return "[type-free] 0";

    const enum cl_type_e code = clt->code;
    switch (code) {
        case CL_TYPE_INT:
            return "[int] 0";

        case CL_TYPE_PTR:
            return "NULL";

        case CL_TYPE_BOOL:
            return "FALSE";

        default:
            return "[?] 0";
    }
}
*/

void jsonifyAuxValue(
        SMGData                       &data,
        const int                       node,
        const TValId                    val,
        const bool                      isField)
{
    const char *label = "NULL";

    switch (val) {
        case VAL_NULL:
        /* TODO (kinst)
            if (isField)
                label = getNullLabelStr(data.sh, static_cast<TFldId>(node));
            */
        (void) isField;
            break;

        case VAL_TRUE:
            label = "TRUE";
            break;

        case VAL_INVALID:
        default:
            label = "VAL_INVALID";
    }

    const int id = ++data.last;
    data.j["values"] += {{"id", id}, {"label", label}, {"lonely", true}};

    data.j["edges"] += {{"label", "hasValue"}, {"from", node}, {"to", id}};
}

void jsonifyHasValue(
        SMGData                       &data,
        const FldHandle                &fld)
{
    SymHeap &sh = data.sh;
    const TValId val = fld.value();
    const bool isField = !hasKey(data.lonelyFields, fld);
    const int idFrom = (isField)
        ? static_cast<int>(fld.fieldId())
        : static_cast<int>(fld.obj());

    if (val <= 0) {
        jsonifyAuxValue(data, idFrom, val, isField);
        return;
    }

    const EValueTarget code = sh.valTarget(val);
    if (VT_CUSTOM == code) {
        jsonifyCustomValue(data, idFrom, val);
        return;
    }

    data.j["edges"] += {{"label", "hasValue"}, {"from", idFrom}, {"to", val}};
}

void jsonifyNeqZero(SMGData &data, const TValId val)
{
    const int id = ++data.last;
    data.j["values"] += {{"id", id}, {"label", "NULL"}, {"lonely", true}};

    data.j["edges"] += {{"label", "neq"}, {"from", val}, {"to", id}};
}

void jsonifyNeqCustom(SMGData &data, const TValId val, const TValId valCustom)
{
    const int id = ++data.last;

    json val_json = describeCustomValue(data, valCustom);
    val_json["id"] = id;
    val_json["lonely"] = true;
    data.j["values"] += val_json;

    data.j["edges"] += {{"label", "neq"}, {"from", val}, {"to", id}};
}

void jsonifyNeq(SMGData &data, const TValId v1, const TValId v2)
{
    data.j["edges"] += {{"label", "neq"}, {"from", v1}, {"to", v2}};
}

class NeqPlotter: public SymPairSet<TValId, /* IREFLEXIVE */ true> {
    public:
        void jsonifyNeqEdges(SMGData &data) {
            for (const TItem &item : cont_) {
                const TValId v1 = item.first;
                const TValId v2 = item.second;

                if (VAL_NULL == v1)
                    jsonifyNeqZero(data, v2);
                else if (VT_CUSTOM == data.sh.valTarget(v2))
                    jsonifyNeqCustom(data, v1, v2);
                else if (VT_CUSTOM == data.sh.valTarget(v1))
                    jsonifyNeqCustom(data, v2, v1);
                else
                    jsonifyNeq(data, v1, v2);
            }
        }
};

void jsonifyNeqEdges(SMGData &data)
{
    SymHeap &sh = data.sh;

    // gather relevant "neq" edges
    NeqPlotter np;
    for (const TValId val : data.values) {
        // go through related values
        TValList relatedVals;
        sh.gatherRelatedValues(relatedVals, val);
        for (const TValId rel : relatedVals)
            if (VAL_NULL == rel
                    || hasKey(data.values, rel)
                    || VT_CUSTOM == sh.valTarget(rel))
                np.add(val, rel);
    }

    // jsonify "neq" edges
    np.jsonifyNeqEdges(data);
}

void jsonifyHasValueEdges(SMGData &data)
{
    // jsonify "hasValue" edges
    for (SMGData::TLiveFields::const_reference item : data.liveFields)
        for (const FldHandle &fld : /* FldList */ item.second)
            jsonifyHasValue(data, fld);

    // jsonify "hasValue" edges for uniform block prototypes
    for (SMGData::TDangValues::const_reference item : data.dangVals) {
        const int id = item.first;
        const TValId val = item.second;

        if (val <= 0) {
            jsonifyAuxValue(data, id, val, /* isField */ false);
            continue;
        }

        data.j["edges"] += {{"label", "hasValue"}, {"from", id}, {"to", val}};
    }
}

void jsonifyEverything(SMGData &data)
{
    jsonifyObjects(data);
    jsonifyAddrs(data);
    jsonifyHasValueEdges(data);
    jsonifyNeqEdges(data);
}

bool smg2jsonCore(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc,
        const TObjSet                   &objs,
        const TValSet                   &vals)
{
    PlotEnumerator *pe = PlotEnumerator::instance();
    std::string jsonName(pe->decorate(name));
    std::string fileName("smg-" + jsonName + ".json");

    // create a json file
    std::fstream out(fileName.c_str(), std::ios::out);
    if (!out) {
        CL_ERROR("unable to create file '" << fileName << "'");
        return false;
    }

    // create the json object
    json j;

    // check whether we can write to stream
    if (!out.flush()) {
        CL_ERROR("unable to write file '" << fileName << "'");
        out.close();
        return false;
    }

    /*
    if (loc)
        CL_NOTE_MSG(loc, "writing heap graph in json to '" << fileName << "'...");
    else
        CL_DEBUG("writing heap graph in json to '" << fileName << "'...");
    */

    int maxId = 0;
    for (const TValId val : vals) {
        if (val > maxId){
            maxId = val;
        }
    }
    for (const TObjId obj : objs) {
        if (obj > maxId){
            maxId = obj;
        }
        FldList liveFields;
        sh.gatherLiveFields(liveFields, obj);
        for (const FldHandle &fld : liveFields) {
            int id = fld.fieldId();
            if (id > maxId){
                maxId = id;
            }
        }
    }
    // initialize an instance of SMGData
    SMGData data(sh, j, objs, vals, maxId);

    data.j["objects"] = json::array();
    data.j["compositeObjects"] = json::array();
    data.j["values"] = json::array();
    data.j["edges"] = json::array();
    /* in case the "__initglobvar" thing is not ideal
    if (loc->line == 0){
        data.j["metadata"] = {{"func_name", ""}};
    } else {
    */
    if (loc && loc->llvm_insn){
        llvm::Instruction *insn = ((llvm::Instruction*) loc->llvm_insn);
        data.j["metadata"] = {{"func_name", insn->getFunction()->getName()}, {"line", loc->line}, {"column", loc->column}, {"file", loc->file}};
    } else if (loc){
        data.j["metadata"] = {{"func_name", "unknown"}, {"line", loc->line}, {"column", loc->column}, {"file", loc->file}};
    } else {
        data.j["metadata"] = {{"func_name", "unknown"}};
    }

    // do our stuff
    jsonifyEverything(data);

    out << j.dump(4) << std::endl;
    // close graph
    const bool ok = !!out;
    out.close();

    return ok;
}

// /////////////////////////////////////////////////////////////////////////////
bool smg2json(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc)
{
    HeapCrawler crawler(sh);

    TObjList allObjs;
    sh.gatherObjects(allObjs);
    for (const TObjId obj : allObjs)
        crawler.digObj(obj);

    const TObjSet objs = crawler.objs();
    const TValSet vals = crawler.vals();

    return smg2jsonCore(sh, name, loc, objs, vals);
}
