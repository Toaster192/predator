#include "config.h"
#include "symplot.hh"

#include <cl/cl_msg.hh>
#include <cl/clutil.hh>
#include <cl/storage.hh>

#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

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

// /////////////////////////////////////////////////////////////////////////////
// implementation of MyHeapCrawler
class MyHeapCrawler {
    public:
        MyHeapCrawler(const SymHeap &sh, const bool digForward = true):
            sh_(const_cast<SymHeap &>(sh)),
            digForward_(digForward)
        {
        }

        bool /* anyChange */ digObj(const TObjId);
        bool /* anyChange */ digVal(const TValId);

        const TObjSet objs() const { return objs_; }
        const TValSet vals() const { return vals_; }

    private:
        void digFields(const TObjId of);
        void operate();

    private:
        SymHeap                    &sh_;
        WorkList<TValId>            wl_;
        bool                        digForward_;
        TObjSet                     objs_;
        TValSet                     vals_;
};

void MyHeapCrawler::digFields(const TObjId obj)
{
    // traverse the outgoing has-value edges
    FldList fields;
    sh_.gatherLiveFields(fields, obj);
    for (const FldHandle &fld : fields)
        wl_.schedule(fld.value());
}

void MyHeapCrawler::operate()
{
    TValId val;
    while (wl_.next(val)) {
        if (val <= VAL_NULL)
            continue;

        // insert the value itself
        vals_.insert(val);
        if (!isAnyDataArea(sh_.valTarget(val)))
            // target is not an object
            continue;

        // insert the target object
        const TObjId obj = sh_.objByAddr(val);
        if (!insertOnce(objs_, obj))
            // the outgoing has-value edges have already been traversed
            continue;

        if (digForward_)
            this->digFields(obj);
    }
}

bool /* anyChange */ MyHeapCrawler::digObj(const TObjId obj)
{
    if (!insertOnce(objs_, obj))
        // the outgoing has-value edges have already been traversed
        return false;

    this->digFields(obj);
    this->operate();
    return true;
}

bool /* anyChange */ MyHeapCrawler::digVal(const TValId val)
{
    if (!wl_.schedule(val))
        return false;

    this->operate();
    return true;
}
//
//// /////////////////////////////////////////////////////////////////////////////
//// implementation of plotHeapCore()
//struct PlotData {
//    typedef std::pair<TObjId, TOffset>                      TFieldKey;
//    typedef std::map<TFieldKey, FldList>                    TLiveFields;
//    typedef std::pair<int /* ID */, TValId>                 TDangVal;
//    typedef std::vector<TDangVal>                           TDangValues;
//
//    SymHeap                            &sh;
//    std::ostream                       &out;
//    const TObjSet                      &objs;
//    const TValSet                      &values;
//    const TIdSet                       *pHighlight;
//    int                                 last;
//    TLiveFields                         liveFields;
//    TFldSet                             lonelyFields;
//    TDangValues                         dangVals;
//
//    PlotData(
//            const SymHeap              &sh_,
//            std::ostream               &out_,
//            const TObjSet              &objs_,
//            const TValSet              &values_,
//            const TIdSet               *pHighlight_):
//        sh(const_cast<SymHeap &>(sh_)),
//        out(out_),
//        objs(objs_),
//        values(values_),
//        pHighlight(pHighlight_),
//        last(0)
//    {
//    }
//};
//
//template <typename TId>
//bool isHighlighted(const PlotData &plot, const TId id)
//{
//    const TIdSet *pSet = plot.pHighlight;
//    return pSet
//        && hasKey(pSet, static_cast<int>(id));
//}
//
//#define GEN_labelByCode(cst) case cst: return #cst
//
//#define SL_QUOTE(what) "\"" << what << "\""
//
//
//const char* myLabelByTargetSpec(const ETargetSpecifier code)
//{
//    switch (code) {
//        GEN_labelByCode(TS_INVALID);
//        GEN_labelByCode(TS_REGION);
//        GEN_labelByCode(TS_FIRST);
//        GEN_labelByCode(TS_LAST);
//        GEN_labelByCode(TS_ALL);
//    }
//
//    CL_BREAK_IF("invalid call of myLabelByTargetSpec()");
//    return "";
//}
//
//inline const char* myOffPrefix(const TOffset off)
//{
//    return (off < 0)
//        ? ""
//        : "+";
//}
//
//#define SIGNED_OFF(off) myOffPrefix(off) << (off)
//
//void plotOffset(PlotData &plot, const TOffset off, const int from, const int to)
//{
//    const char *color = (off < 0)
//        ? "red"
//        : "black";
//
//    plot.out << "\t"
//        << SL_QUOTE(from) << " -> " << SL_QUOTE(to)
//        << " [color=" << color
//        << ", fontcolor=" << color
//        << ", label=\"[" << SIGNED_OFF(off)
//        << "]\"];\n";
//}
//
//
//const char* myLabelByOrigin(const EValueOrigin code)
//{
//    switch (code) {
//        GEN_labelByCode(VO_INVALID);
//        GEN_labelByCode(VO_ASSIGNED);
//        GEN_labelByCode(VO_UNKNOWN);
//        GEN_labelByCode(VO_REINTERPRET);
//        GEN_labelByCode(VO_DEREF_FAILED);
//        GEN_labelByCode(VO_STACK);
//        GEN_labelByCode(VO_HEAP);
//    }
//
//    CL_BREAK_IF("invalid call of myLabelByOrigin()");
//    return "";
//}
//
//const char* myLabelByTarget(const EValueTarget code)
//{
//    switch (code) {
//        GEN_labelByCode(VT_INVALID);
//        GEN_labelByCode(VT_UNKNOWN);
//        GEN_labelByCode(VT_COMPOSITE);
//        GEN_labelByCode(VT_CUSTOM);
//        GEN_labelByCode(VT_OBJECT);
//        GEN_labelByCode(VT_RANGE);
//    }
//
//    CL_BREAK_IF("invalid call of myLabelByTarget()");
//    return "";
//}
//
//void describeInt(PlotData &plot, const IR::TInt num, const TValId val)
//{
//    plot.out << ", fontcolor=red, label=\"[int] " << num;
//    if (IR::Int0 < num && num < UCHAR_MAX && isprint(num))
//        plot.out << " = '" << static_cast<char>(num) << "'";
//
//    plot.out << " (#" << val << ")\"";
//}
//
//void describeIntRange(PlotData &plot, const IR::Range &rng, const TValId val)
//{
//    plot.out << ", fontcolor=blue, label=\"[int range] ";
//
//    myPrintRawRange(plot.out, rng);
//    
//    plot.out << " (#" << val << ")\"";
//}
//
//void describeReal(PlotData &plot, const float fpn, const TValId val)
//{
//    plot.out << ", fontcolor=red, label=\"[real] "
//        << fpn << " (#"
//        << val << ")\"";
//}
//
//void describeFnc(PlotData &plot, const cl_uid_t uid, const TValId val)
//{
//    TStorRef stor = plot.sh.stor();
//    const CodeStorage::Fnc *fnc = stor.fncs[uid];
//    CL_BREAK_IF(!fnc);
//
//    const std::string name = nameOf(*fnc);
//    plot.out << ", fontcolor=chartreuse2, label=\""
//        << name << "() (#"
//        << val << ")\"";
//}
//
//void describeStr(PlotData &plot, const std::string &str, const TValId val)
//{
//    // we need to escape twice, once for the C compiler and once for graphviz
//    plot.out << ", fontcolor=blue, label=\"\\\""
//        << str << "\\\" (#"
//        << val << ")\"";
//}
//
//void describeCustomValue(PlotData &plot, const TValId val)
//{
//    SymHeap &sh = plot.sh;
//    const CustomValue cVal = sh.valUnwrapCustom(val);
//
//    const ECustomValue code = cVal.code();
//    switch (code) {
//        case CV_INVALID:
//            plot.out << ", fontcolor=red, label=CV_INVALID";
//            break;
//
//        case CV_INT_RANGE: {
//            const IR::Range &rng = cVal.rng();
//            if (isSingular(rng))
//                describeInt(plot, rng.lo, val);
//            else
//                describeIntRange(plot, rng, val);
//            break;
//        }
//
//        case CV_REAL:
//            describeReal(plot, cVal.fpn(), val);
//            break;
//
//        case CV_FNC:
//            describeFnc(plot, cVal.uid(), val);
//            break;
//
//        case CV_STRING:
//            describeStr(plot, cVal.str(), val);
//            break;
//    }
//}
//
//void plotCustomValue(
//        PlotData                       &plot,
//        const int                       idFrom,
//        const TValId                    val)
//{
//    const int id = ++plot.last;
//    plot.out << "\t" << SL_QUOTE("lonely" << id) << " [shape=plaintext";
//
//    describeCustomValue(plot, val);
//
//    plot.out << "];\n\t"
//        << SL_QUOTE(idFrom)
//        << " -> " << SL_QUOTE("lonely" << id)
//        << " [color=blue, fontcolor=blue"
//        << "];\n";
//}
//
//void plotSingleValue(PlotData &plot, const TValId val)
//{
//    SymHeap &sh = plot.sh;
//
//    const char *color = "black";
//    const char *suffix = 0;
//
//    const TObjId obj = sh.objByAddr(val);
//    const EStorageClass sc = sh.objStorClass(obj);
//
//    const EValueTarget code = sh.valTarget(val);
//    switch (code) {
//        case VT_CUSTOM:
//            // skip it, custom values are now handled in plotHasValue()
//            return;
//
//        case VT_OBJECT:
//            break;
//
//        case VT_INVALID:
//        case VT_COMPOSITE:
//        case VT_RANGE:
//            color = "red";
//            break;
//
//        case VT_UNKNOWN:
//            suffix = myLabelByOrigin(sh.valOrigin(val));
//            // fall through!
//            goto preserve_suffix;
//    }
//
//    switch (sc) {
//        case SC_INVALID:
//        case SC_UNKNOWN:
//            color = "red";
//            break;
//
//        case SC_STATIC:
//        case SC_ON_STACK:
//            color = "blue";
//            break;
//
//        case SC_ON_HEAP:
//            goto preserve_suffix;
//    }
//
//    suffix = myLabelByTarget(code);
//preserve_suffix:
//
//    const ETargetSpecifier ts = sh.targetSpec(val);
//    if (TS_REGION != ts)
//        color = "chartreuse2";
//
//    const float pw = static_cast<float>(1U + sh.usedByCount(val));
//    plot.out << "\t" << SL_QUOTE(val)
//        << " [shape=ellipse, penwidth=" << pw
//        << ", fontcolor=" << color
//        << ", label=\"#" << val;
//
//    if (suffix)
//        plot.out << " " << suffix;
//
//    if (isAnyDataArea(code)) {
//        const IR::Range &offRange = sh.valOffsetRange(val);
//        plot.out << " [off = ";
//        myPrintRawRange(plot.out, offRange);
//
//        const ETargetSpecifier ts = sh.targetSpec(val);
//        if (TS_REGION != ts)
//            plot.out << ", " << myLabelByTargetSpec(ts);
//
//        plot.out << ", obj = #" << obj << "]";
//    }
//
//    plot.out << "\"];\n";
//}
//
//void plotPointsTo(PlotData &plot, const TValId val, const TFldId target)
//{
//    plot.out << "\t" << SL_QUOTE(val)
//        << " -> " << SL_QUOTE(target)
//        << " [color=chartreuse2, fontcolor=chartreuse2];\n";
//}
//
//void plotRangePtr(PlotData &plot, TValId val, TObjId obj)
//{
//    plot.out << "\t" << SL_QUOTE(val) << " -> "
//        << SL_QUOTE(obj)
//        << " [color=red, fontcolor=red];\n";
//}
//
//void plotAddrs(PlotData &plot)
//{
//    SymHeap &sh = plot.sh;
//
//    for (const TValId val : plot.values) {
//        // plot a value node
//        plotSingleValue(plot, val);
//
//        const TObjId obj = sh.objByAddr(val);
//
//        const EValueTarget code = sh.valTarget(val);
//        switch (code) {
//            case VT_OBJECT:
//                break;
//
//            case VT_RANGE:
//                plotRangePtr(plot, val, obj);
//                continue;
//
//            default:
//                continue;
//        }
//
//        const TOffset off = sh.valOffset(val);
//        if (off) {
//            const PlotData::TFieldKey key(obj, off);
//            PlotData::TLiveFields::const_iterator it = plot.liveFields.find(key);
//            if ((plot.liveFields.end() != it) && (1 == it->second.size())) {
//                // plot the target field as an abbreviation
//                const FldHandle &target = it->second.front();
//                plotPointsTo(plot, val, target.fieldId());
//                continue;
//            }
//        }
//
//        plotOffset(plot, off, val, obj);
//    }
//
//    // go through value prototypes used in uniform blocks
//    for (PlotData::TDangValues::const_reference item : plot.dangVals) {
//        const TValId val = item.second;
//        if (val <= 0)
//            continue;
//
//        // plot a value node
//        CL_BREAK_IF(isAnyDataArea(sh.valTarget(val)));
//        plotSingleValue(plot, val);
//    }
//}
//
//const char* valNullLabel(const SymHeapCore &sh, const TFldId fld)
//{
//    const FldHandle hdl(const_cast<SymHeapCore &>(sh), fld);
//    const TObjType clt = hdl.type();
//    if (!clt)
//        return "[type-free] 0";
//
//    const enum cl_type_e code = clt->code;
//    switch (code) {
//        case CL_TYPE_INT:
//            return "[int] 0";
//
//        case CL_TYPE_PTR:
//            return "NULL";
//
//        case CL_TYPE_BOOL:
//            return "FALSE";
//
//        default:
//            return "[?] 0";
//    }
//}
//
//void plotAuxValue(
//        PlotData                       &plot,
//        const int                       node,
//        const TValId                    val,
//        const bool                      isField,
//        const bool                      isLonely = false)
//{
//    const char *color = "blue";
//    const char *label = "NULL";
//
//    switch (val) {
//        case VAL_NULL:
//            if (isField)
//                label = valNullLabel(plot.sh, static_cast<TFldId>(node));
//            break;
//
//        case VAL_TRUE:
//            color = "orange";
//            label = "TRUE";
//            break;
//
//        case VAL_INVALID:
//        default:
//            color = "red";
//            label = "VAL_INVALID";
//    }
//
//    const int id = ++plot.last;
//    plot.out << "\t" << SL_QUOTE("lonely" << id)
//        << " [shape=plaintext, fontcolor=" << color
//        << ", label=" << SL_QUOTE(label) << "];\n";
//
//    const char *prefix = "";
//    if (isLonely)
//        prefix = "lonely";
//
//    plot.out << "\t" << SL_QUOTE(prefix << node)
//        << " -> " << SL_QUOTE("lonely" << id)
//        << " [color=blue, fontcolor=blue];\n";
//}
//
//void plotHasValue(
//        PlotData                       &plot,
//        const FldHandle                &fld)
//{
//    SymHeap &sh = plot.sh;
//    const TValId val = fld.value();
//    const bool isField = !hasKey(plot.lonelyFields, fld);
//    const int idFrom = (isField)
//        ? static_cast<int>(fld.fieldId())
//        : static_cast<int>(fld.obj());
//
//    if (val <= 0) {
//        plotAuxValue(plot, idFrom, val, isField);
//        return;
//    }
//
//    const EValueTarget code = sh.valTarget(val);
//    if (VT_CUSTOM == code) {
//        plotCustomValue(plot, idFrom, val);
//        return;
//    }
//
//    plot.out << "\t"
//        << SL_QUOTE(idFrom)
//        << " -> " << SL_QUOTE(val)
//        << " [color=blue, fontcolor=blue];\n";
//}
//
//void plotNeqZero(PlotData &plot, const TValId val)
//{
//    const int id = ++plot.last;
//    plot.out << "\t" << SL_QUOTE("lonely" << id)
//        << " [shape=plaintext, fontcolor=blue, label=NULL];\n";
//
//    plot.out << "\t" << SL_QUOTE(val)
//        << " -> " << SL_QUOTE("lonely" << id)
//        << " [color=red, fontcolor=orange, label=neq style=dashed"
//        ", penwidth=2.0];\n";
//}
//
//void plotNeqCustom(PlotData &plot, const TValId val, const TValId valCustom)
//{
//    const int id = ++plot.last;
//    plot.out << "\t" << SL_QUOTE("lonely" << id)
//        << " [shape=plaintext";
//
//    describeCustomValue(plot, valCustom);
//
//    plot.out << "];\n\t" << SL_QUOTE(val)
//        << " -> " << SL_QUOTE("lonely" << id)
//        << " [color=red, fontcolor=orange, label=neq style=dashed"
//        ", penwidth=2.0];\n";
//}
//
//void plotNeq(std::ostream &out, const TValId v1, const TValId v2)
//{
//    out << "\t" << SL_QUOTE(v1)
//        << " -> " << SL_QUOTE(v2)
//        << " [color=red, style=dashed, penwidth=2.0, arrowhead=none"
//        ", label=neq, fontcolor=orange, constraint=false];\n";
//}
//
//class NeqPlotter: public SymPairSet<TValId, /* IREFLEXIVE */ true> {
//    public:
//        void plotNeqEdges(PlotData &plot) {
//            for (const TItem &item : cont_) {
//                const TValId v1 = item.first;
//                const TValId v2 = item.second;
//
//                if (VAL_NULL == v1)
//                    plotNeqZero(plot, v2);
//                else if (VT_CUSTOM == plot.sh.valTarget(v2))
//                    plotNeqCustom(plot, v1, v2);
//                else if (VT_CUSTOM == plot.sh.valTarget(v1))
//                    plotNeqCustom(plot, v2, v1);
//                else
//                    plotNeq(plot.out, v1, v2);
//            }
//        }
//};
//
//void plotNeqEdges(PlotData &plot)
//{
//    SymHeap &sh = plot.sh;
//
//    // gather relevant "neq" edges
//    NeqPlotter np;
//    for (const TValId val : plot.values) {
//        // go through related values
//        TValList relatedVals;
//        sh.gatherRelatedValues(relatedVals, val);
//        for (const TValId rel : relatedVals)
//            if (VAL_NULL == rel
//                    || hasKey(plot.values, rel)
//                    || VT_CUSTOM == sh.valTarget(rel))
//                np.add(val, rel);
//    }
//
//    // plot "neq" edges
//    np.plotNeqEdges(plot);
//}
//
//void plotHasValueEdges(PlotData &plot)
//{
//    // plot "hasValue" edges
//    for (PlotData::TLiveFields::const_reference item : plot.liveFields)
//        for (const FldHandle &fld : /* FldList */ item.second)
//            plotHasValue(plot, fld);
//
//    // plot "hasValue" edges for uniform block prototypes
//    for (PlotData::TDangValues::const_reference item : plot.dangVals) {
//        const int id = item.first;
//        const TValId val = item.second;
//
//        if (val <= 0) {
//            plotAuxValue(plot, id, val, /* isField */ false, /* lonely */ true);
//            continue;
//        }
//
//        plot.out << "\t" << SL_QUOTE("lonely" << id)
//            << " -> " << SL_QUOTE(val)
//            << " [color=blue, fontcolor=blue];\n";
//    }
//}
//
//void plotEverything(PlotData &plot)
//{
//    plotObjects(plot);
//    plotAddrs(plot);
//    plotHasValueEdges(plot);
//    plotNeqEdges(plot);
//}
//
//bool plotHeapCore(
//        const SymHeap                   &sh,
//        const std::string               &name,
//        const struct cl_loc             *loc,
//        const TObjSet                   &objs,
//        const TValSet                   &vals,
//        std::string                     *pName = 0,
//        const TIdSet                    *pHighlight = 0)
//{
//    PlotEnumerator *pe = PlotEnumerator::instance();
//    std::string plotName(pe->decorate(name));
//    std::string fileName(plotName + ".dot");
//
//    if (pName)
//        // propagate the resulting name back to the caller
//        *pName = plotName;
//
//    // create a dot file
//    std::fstream out(fileName.c_str(), std::ios::out);
//    if (!out) {
//        CL_ERROR("unable to create file '" << fileName << "'");
//        return false;
//    }
//
//    // open graph
//    out << "digraph " << SL_QUOTE(plotName)
//        << " {\n\tlabel=<<FONT POINT-SIZE=\"18\">" << plotName
//        << "</FONT>>;\n\tclusterrank=local;\n\tlabelloc=t;\n";
//
//    // check whether we can write to stream
//    if (!out.flush()) {
//        CL_ERROR("unable to write file '" << fileName << "'");
//        out.close();
//        return false;
//    }
//
//    if (loc)
//        CL_NOTE_MSG(loc, "writing heap graph to '" << fileName << "'...");
//    else
//        CL_DEBUG("writing heap graph to '" << fileName << "'...");
//
//    // initialize an instance of PlotData
//    PlotData plot(sh, out, objs, vals, pHighlight);
//
//    // do our stuff
//    plotEverything(plot);
//
//    // close graph
//    out << "}\n";
//    const bool ok = !!out;
//    out.close();
//    return ok;
//}
//
//// /////////////////////////////////////////////////////////////////////////////
//// implementation of plotHeap()
//bool plotHeap(
//        const SymHeap                   &sh,
//        const std::string               &name,
//        const struct cl_loc             *loc,
//        const TValList                  &startingPoints)
//{
//    MyHeapCrawler crawler(sh);
//
//    for (const TValId val : startingPoints)
//        crawler.digVal(val);
//
//    return plotHeapCore(sh, name, loc, crawler.objs(), crawler.vals());
//}
//
//bool plotHeap(
//        const SymHeap                   &sh,
//        const std::string               &name,
//        const struct cl_loc             *loc,
//        std::string                     *pName,
//        const TIdSet                    *pHighlight)
//{
//    MyHeapCrawler crawler(sh);
//
//    TObjList allObjs;
//    sh.gatherObjects(allObjs);
//    for (const TObjId obj : allObjs)
//        crawler.digObj(obj);
//
//    const TObjSet objs = crawler.objs();
//    const TValSet vals = crawler.vals();
//
//    return plotHeapCore(sh, name, loc, objs, vals, pName, pHighlight);
//}
//
//bool plotHeap(
//        const SymHeap                   &sh,
//        const std::string               &name,
//        const struct cl_loc             *loc,
//        const TObjSet                   &objs)
//{
//    MyHeapCrawler crawler(sh, /* digForward */ false);
//
//    for (const TObjId obj : objs)
//        crawler.digObj(obj);
//
//    return plotHeapCore(sh, name, loc, crawler.objs(), crawler.vals());
//}

// SEPARATOR
// SEPARATOR
// SEPARATOR
// SEPARATOR
// SEPARATOR
// SEPARATOR
// SEPARATOR


enum EFieldClass {
    FC_VOID = 0,
    FC_PTR,
    FC_NEXT,
    FC_PREV,
    FC_DATA
};

struct FieldWrapper {
    FldHandle       fld;
    EFieldClass     code;

    FieldWrapper():
        code(FC_VOID)
    {
    }

    FieldWrapper(const FldHandle &obj_, EFieldClass code_):
        fld(obj_),
        code(code_)
    {
    }

    FieldWrapper(const FldHandle &obj_):
        fld(obj_),
        code(isDataPtr(fld.type())
            ? FC_PTR
            : FC_DATA)
    {
    }
};

struct PTAData {
    typedef std::pair<TObjId, TOffset>                      TFieldKey;
    typedef std::map<TFieldKey, FldList>                    TLiveFields;
    typedef std::pair<int /* ID */, TValId>                 TDangVal;
    typedef std::vector<TDangVal>                           TDangValues;

    SymHeap                            &sh;
    std::ostream                       &out;
    const TObjSet                      &objs;
    const TValSet                      &values;
    const TIdSet                       *pHighlight;
    int                                 last;
    TLiveFields                         liveFields;
    TFldSet                             lonelyFields;
    TDangValues                         dangVals;

    PTAData(
            const SymHeap              &sh_,
            std::ostream               &out_,
            const TObjSet              &objs_,
            const TValSet              &values_,
            const TIdSet               *pHighlight_):
        sh(const_cast<SymHeap &>(sh_)),
        out(out_),
        objs(objs_),
        values(values_),
        pHighlight(pHighlight_),
        last(0)
    {
    }
};

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


#define GEN_labelByCode(cst) case cst: return #cst

#define SL_QUOTE(what) "\"" << what << "\""

inline const char* myOffPrefix(const TOffset off)
{
    return (off < 0)
        ? ""
        : "+";
}

#define SIGNED_OFF(off) myOffPrefix(off) << (off)

std::string myLabelOfCompObj(const SymHeap &sh, const TObjId obj, bool showProps)
{
    std::ostringstream label;
    const TProtoLevel protoLevel= sh.objProtoLevel(obj);
    if (protoLevel)
        label << "[L" << protoLevel << " prototype] ";

    const EObjKind kind = sh.objKind(obj);
    switch (kind) {
        case OK_REGION:
            return label.str();

        case OK_OBJ_OR_NULL:
        case OK_SEE_THROUGH:
        case OK_SEE_THROUGH_2N:
            label << "0..1";
            break;

        case OK_SLS:
            label << "SLS";
            break;

        case OK_DLS:
            label << "DLS";
            break;
    }

    switch (kind) {
        case OK_SLS:
        case OK_DLS:
            // append minimal segment length
            label << " " << sh.segMinLength(obj) << "+";

        default:
            break;
    }

    if (showProps && OK_OBJ_OR_NULL != kind) {
        const BindingOff &bf = sh.segBinding(obj);
        switch (kind) {
            case OK_SLS:
            case OK_DLS:
                label << ", head [" << SIGNED_OFF(bf.head) << "]";

            default:
                break;
        }

        switch (kind) {
            case OK_SEE_THROUGH:
            case OK_SLS:
            case OK_DLS:
                label << ", next [" << SIGNED_OFF(bf.next) << "]";

            default:
                break;
        }

        if (OK_DLS == kind)
            label << ", prev [" << SIGNED_OFF(bf.prev) << "]";
    }

    return label.str();
}

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


void extractUniformBlocks(PTAData &ptadata, const TObjId obj)
{
    SymHeap &sh = ptadata.sh;

    // get all uniform blocks inside the given object
    TUniBlockMap bMap;
    sh.gatherUniformBlocks(bMap, obj);

    // plot all uniform blocks
    for (TUniBlockMap::const_reference item : bMap) {
        const UniformBlock &bl = item.second;

        // plot block node
        const int id = ++ptadata.last;
        ptadata.out << "\t" << SL_QUOTE("lonely" << id)
            << " [shape=box, color=blue, fontcolor=blue, label=\"UNIFORM_BLOCK "
            << bl.size << "B\"];\n";

        // plot offset edge
        const TOffset off = bl.off;
        CL_BREAK_IF(off < 0);
        ptadata.out << "\t" << SL_QUOTE(obj)
            << " -> " << SL_QUOTE("lonely" << id)
            << " [color=black, fontcolor=black, label=\"[+"
            << off << "]\"];\n";

        // schedule hasValue edge
        const PTAData::TDangVal dv(id, bl.tplValue);
        ptadata.dangVals.push_back(dv);
    }
}

void describeFieldPlacement(PTAData &ptadata, const FldHandle &fld, TObjType clt)
{
    const TObjType cltField = fld.type();
    if (!cltField || *cltField == *clt)
        // nothing interesting here
        return;

    // read field offset
    const TOffset off = fld.offset();

    TFieldIdxChain ic;
    if (!myDigIcByOffset(&ic, clt, cltField, off))
        // type of the field not found in clt
        return;

    // chain of indexes found!
    for (const int idx : ic) {
        CL_BREAK_IF(clt->item_cnt <= idx);
        const cl_type_item *item = clt->items + idx;
        const cl_type_e code = clt->code;

        if (CL_TYPE_ARRAY == code) {
            // TODO: support non-zero indexes? (not supported by CltFinder yet)
            CL_BREAK_IF(item->offset);
            ptadata.out << "[0]";
        }
        else {
            // read field name
            const char *name = item->name;
            if (!name)
                name = "<anon>";

            ptadata.out << "." << name;
        }

        // jump to the next item
        clt = item->type;
    }
}

void describeVarCore(int *pInst, PTAData &ptadata, const TObjId obj)
{
    SymHeap &sh = ptadata.sh;
    TStorRef stor = sh.stor();

    CallInst ci(-1, -1);
    if (sh.isAnonStackObj(obj, &ci)) {
        // anonymous stack object
        ptadata.out << "STACK of ";
        if (-1 == ci.uid)
            ptadata.out << "FNC_INVALID";
        else
            ptadata.out << nameOf(*stor.fncs[ci.uid]) << "()";
        *pInst = ci.inst;
    }
    else {
        // var lookup
        const CVar cv = sh.cVarByObject(obj);
        ptadata.out << "CL" << varToString(stor, cv.uid);
        //const CodeStorage::Var &var = stor.vars[cv.uid];
	//llvm::errs() << "foo: " << *((llvm::Value*) var.llvmValue);
	//llvm::errs() << "bar: " << *((llvm::Instruction*) var.loc.llvm_insn);
        *pInst = cv.inst;
    }
}

void describeVar(PTAData &ptadata, const TObjId obj)
{
    if (OBJ_RETURN == obj) {
        ptadata.out << "OBJ_RETURN";
        return;
    }

    int inst;
    if (ptadata.sh.isValid(obj))
        describeVarCore(&inst, ptadata, obj);
    else
        inst = -1;

    ptadata.out << " [obj = #" << obj;
    if (1 < inst)
        ptadata.out << ", inst = " << inst;
    ptadata.out << "]";
}

void describeField(PTAData &ptadata, const FldHandle &fld, const bool lonely)
{
    SymHeap &sh = ptadata.sh;
    const TObjId obj = fld.obj();
    const EStorageClass code = sh.objStorClass(obj);

    const char *tag = "";
    if (lonely && isProgramVar(code)) {
        describeVar(ptadata, obj);
        tag = "field";
    }

    const TObjType cltRoot = sh.objEstimatedType(obj);
    if (cltRoot)
        describeFieldPlacement(ptadata, fld, cltRoot);

    ptadata.out << " " << tag << "#" << fld.fieldId();
}


bool extractField(PTAData &ptadata, const FieldWrapper &fw, const bool lonely)
{
    SymHeap &sh = ptadata.sh;

    const FldHandle &fld = fw.fld;
    CL_BREAK_IF(!fld.isValidHandle());

    const char *color = "black";
    const char *props = ", penwidth=3.0, style=dashed";

    const EFieldClass code = fw.code;
    switch (code) {
        case FC_VOID:
            CL_BREAK_IF("extractField() got an object of class FC_VOID");
            return false;

        case FC_PTR:
            props = "";
            break;

        case FC_NEXT:
            color = "red";
            break;

        case FC_PREV:
            color = "orange";
            break;

        case FC_DATA:
            color = "gray";
            props = ", style=dotted";
    }

    // update filed lookup
    const TObjId obj = fld.obj();
    const PTAData::TFieldKey key(obj, fld.offset());
    ptadata.liveFields[key].push_back(fld);

    int id = fld.fieldId();

    if (lonely) {
        id = obj;

        const EStorageClass code = sh.objStorClass(obj);
        switch (code) {
            case SC_STATIC:
            case SC_ON_STACK:
                color = "blue";
                break;

            default:
                break;
        }
    }

    ptadata.out << "\t" << SL_QUOTE(id)
        << " [shape=box, color=" << color
        << ", fontcolor=" << color << props
        << ", label=\"";

    describeField(ptadata, fld, lonely);

    if (FC_DATA == code)
        ptadata.out << " [size = " << fld.type()->size << "B]";

    ptadata.out << "\"];\n";
    return true;
}

void extractOffset(PTAData &ptadata, const TOffset off, const int from, const int to)
{
    const char *color = (off < 0)
        ? "red"
        : "black";

    ptadata.out << "\t"
        << SL_QUOTE(from) << " -> " << SL_QUOTE(to)
        << " [color=" << color
        << ", fontcolor=" << color
        << ", label=\"[" << SIGNED_OFF(off)
        << "]\"];\n";
}


template <class TCont>
void extractFields(PTAData &ptadata, const TObjId obj, const TCont &liveFields)
{
    SymHeap &sh = ptadata.sh;

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

    // plot all atomic objects inside
    for (TAtomByOff::const_reference item : objByOff) {
        const TOffset off = item.first;
        for (const FieldWrapper &fw : /* TAtomList */ item.second) {
            // plot a single object
            if (!extractField(ptadata, fw, /* lonely */ false))
                continue;

            // connect the field with the object by an offset edge
            extractOffset(ptadata, off, obj, fw.fld.fieldId());
        }
    }
}


const char* myLabelByOrigin(const EValueOrigin code)
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

    CL_BREAK_IF("invalid call of myLabelByOrigin()");
    return "";
}

const char* myLabelByTarget(const EValueTarget code)
{
    switch (code) {
        GEN_labelByCode(VT_INVALID);
        GEN_labelByCode(VT_UNKNOWN);
        GEN_labelByCode(VT_COMPOSITE);
        GEN_labelByCode(VT_CUSTOM);
        GEN_labelByCode(VT_OBJECT);
        GEN_labelByCode(VT_RANGE);
    }

    CL_BREAK_IF("invalid call of myLabelByTarget()");
    return "";
}

const char* myLabelByTargetSpec(const ETargetSpecifier code)
{
    switch (code) {
        GEN_labelByCode(TS_INVALID);
        GEN_labelByCode(TS_REGION);
        GEN_labelByCode(TS_FIRST);
        GEN_labelByCode(TS_LAST);
        GEN_labelByCode(TS_ALL);
    }

    CL_BREAK_IF("invalid call of myLabelByTargetSpec()");
    return "";
}

void myPrintRawInt(
        std::ostream                &str,
        const IR::TInt               i,
        const char                  *suffix = "")
{
    if (IR::IntMin == i)
        str << "-inf";
    else if (IR::IntMax == i)
        str << "inf";
    else
        str << i;

    str << suffix;
}

void myPrintRawRange(
        std::ostream                &str,
        const IR::Range             &rng,
        const char                  *suffix = "")
{
    if (isSingular(rng)) {
        str << rng.lo << suffix;
        return;
    }

    myPrintRawInt(str, rng.lo, suffix);
    str << " .. ";
    myPrintRawInt(str, rng.hi, suffix);

    if (isAligned(rng))
        str << ", alignment = " << rng.alignment << suffix;
}

const char* myLabelByStorClass(const EStorageClass code)
{
    switch (code) {
        GEN_labelByCode(SC_INVALID);
        GEN_labelByCode(SC_UNKNOWN);
        GEN_labelByCode(SC_STATIC);
        GEN_labelByCode(SC_ON_HEAP);
        GEN_labelByCode(SC_ON_STACK);
    }

    CL_BREAK_IF("invalid call of myLabelByStorClass()");
    return "";
}

void extractRawObject(PTAData &ptadata, const TObjId obj, const char *color)
{
    SymHeap &sh = ptadata.sh;
    const TSizeRange size = sh.objSize(obj);

    const bool isValid = sh.isValid(obj);
    if (!isValid)
        color = "red";

    ptadata.out << "\t" << SL_QUOTE(obj)
        << " [shape=box";

    if (!sh.isValid(obj))
        ptadata.out << "[INVALID] ";

    const EStorageClass code = sh.objStorClass(obj);
    if (isProgramVar(code))
        describeVar(ptadata, obj);
    else
        ptadata.out << "#" << obj;

    ptadata.out << " [" << myLabelByStorClass(code) << ", size = ";
    myPrintRawRange(ptadata.out, size, " B");
    ptadata.out << "]\"];\n";
}


bool extractLonelyField(PTAData &ptadata, const FldHandle &fld)
{
    SymHeap &sh = ptadata.sh;

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

    ptadata.lonelyFields.insert(fld);

    const FieldWrapper fw(fld);
    extractField(ptadata, fw, /* lonely */ true);
    return true;
}

template <class TCont>
void extractCompositeObj(PTAData &ptadata, const TObjId obj, const TCont &liveFields)
{
    SymHeap &sh = ptadata.sh;

    const char *color = "black";
    const char *pw = "1.0";

    const EStorageClass code = sh.objStorClass(obj);
    switch (code) {
        case SC_INVALID:
        case SC_UNKNOWN:
            color = "red";
            break;

        case SC_STATIC:
        case SC_ON_STACK:
            color = "blue";
            break;

        case SC_ON_HEAP:
            break;
    }

    const EObjKind kind = sh.objKind(obj);
    switch (kind) {
        case OK_REGION:
            break;

        case OK_OBJ_OR_NULL:
        case OK_SEE_THROUGH:
        case OK_SEE_THROUGH_2N:
            color = "chartreuse2";
            pw = "3.0";
            break;

        case OK_SLS:
            color = "red";
            pw = "3.0";
            break;

        case OK_DLS:
            color = "orange";
            pw = "3.0";
            break;
    }

    const std::string label = myLabelOfCompObj(sh, obj, /* showProps */ true);

    // open cluster
    ptadata.out
        << "\" {\n\tlabel=" << SL_QUOTE(label)
        << ";\n\tstyle=dashed;\n";

    extractRawObject(ptadata, obj, color);

    // plot all uniform blocks
    extractUniformBlocks(ptadata, obj);

    // plot all atomic objects inside
    extractFields(ptadata, obj, liveFields);

    // close cluster
    ptadata.out << "}\n";
}

void extractObjects(PTAData &ptadata)
{
    SymHeap &sh = ptadata.sh;

    // go through roots
    for (const TObjId obj : ptadata.objs) {
        // gather live objects
        FldList liveFields;
        sh.gatherLiveFields(liveFields, obj);

        if (OK_REGION == sh.objKind(obj)
                && (1 == liveFields.size())
                && extractLonelyField(ptadata, liveFields.front()))
            // this one went out in a simplified form
            continue;

        extractCompositeObj(ptadata, obj, liveFields);
    }
}

void extractSingleValue(PTAData &ptadata, const TValId val)
{
    SymHeap &sh = ptadata.sh;

    const char *color = "black";
    const char *suffix = 0;

    const TObjId obj = sh.objByAddr(val);
    const EStorageClass sc = sh.objStorClass(obj);

    const EValueTarget code = sh.valTarget(val);
    switch (code) {
        case VT_CUSTOM:
            // skip it, custom values are now handled in plotHasValue()
            return;

        case VT_OBJECT:
            break;

        case VT_INVALID:
        case VT_COMPOSITE:
        case VT_RANGE:
            color = "red";
            break;

        case VT_UNKNOWN:
            suffix = myLabelByOrigin(sh.valOrigin(val));
            // fall through!
            goto preserve_suffix;
    }

    switch (sc) {
        case SC_INVALID:
        case SC_UNKNOWN:
            color = "red";
            break;

        case SC_STATIC:
        case SC_ON_STACK:
            color = "blue";
            break;

        case SC_ON_HEAP:
            goto preserve_suffix;
    }

    suffix = myLabelByTarget(code);
preserve_suffix:

    const ETargetSpecifier ts = sh.targetSpec(val);
    if (TS_REGION != ts)
        color = "chartreuse2";

    const float pw = static_cast<float>(1U + sh.usedByCount(val));
    ptadata.out << "\t" << SL_QUOTE(val)
        << " [shape=ellipse, penwidth=" << pw
        << ", fontcolor=" << color
        << ", label=\"#" << val;

    if (suffix)
        ptadata.out << " " << suffix;

    if (isAnyDataArea(code)) {
        const IR::Range &offRange = sh.valOffsetRange(val);
        ptadata.out << " [off = ";
        myPrintRawRange(ptadata.out, offRange);

        const ETargetSpecifier ts = sh.targetSpec(val);
        if (TS_REGION != ts)
            ptadata.out << ", " << myLabelByTargetSpec(ts);

        ptadata.out << ", obj = #" << obj << "]";
    }

    ptadata.out << "\"];\n";
}

void extractPointsTo(PTAData &ptadata, const TValId val, const TFldId target)
{
    ptadata.out << "\t" << SL_QUOTE(val)
        << " -> " << SL_QUOTE(target)
        << " [color=chartreuse2, fontcolor=chartreuse2];\n";
}

void extractRangePtr(PTAData &ptadata, TValId val, TObjId obj)
{
    ptadata.out << "\t" << SL_QUOTE(val) << " -> "
        << SL_QUOTE(obj)
        << " [color=red, fontcolor=red];\n";
}

void extractAddrs(PTAData &ptadata)
{
    SymHeap &sh = ptadata.sh;

    for (const TValId val : ptadata.values) {
        // extract a value node
        extractSingleValue(ptadata, val);

        const TObjId obj = sh.objByAddr(val);

        const EValueTarget code = sh.valTarget(val);
        switch (code) {
            case VT_OBJECT:
                break;

            case VT_RANGE:
                extractRangePtr(ptadata, val, obj);
                continue;

            default:
                continue;
        }

        const TOffset off = sh.valOffset(val);
        if (off) {
            const PTAData::TFieldKey key(obj, off);
            PTAData::TLiveFields::const_iterator it = ptadata.liveFields.find(key);
            if ((ptadata.liveFields.end() != it) && (1 == it->second.size())) {
                // ptadata the target field as an abbreviation
                const FldHandle &target = it->second.front();
                extractPointsTo(ptadata, val, target.fieldId());
                continue;
            }
        }

        extractOffset(ptadata, off, val, obj);
    }

    // go through value prototypes used in uniform blocks
    for (PTAData::TDangValues::const_reference item : ptadata.dangVals) {
        const TValId val = item.second;
        if (val <= 0)
            continue;

        // ptadata a value node
        CL_BREAK_IF(isAnyDataArea(sh.valTarget(val)));
        extractSingleValue(ptadata, val);
    }
}

void extractEverything(PTAData &ptadata)
{
    extractObjects(ptadata);
    extractAddrs(ptadata);
    //plotHasValueEdges(plot);
    //plotNeqEdges(plot);
}

bool extractPTACore(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc,
        const TObjSet                   &objs,
        const TValSet                   &vals,
        std::string                     *pName = 0,
        const TIdSet                    *pHighlight = 0)
{
    PlotEnumerator *pe = PlotEnumerator::instance();
    std::string plotName(pe->decorate(name));
    std::string fileName(plotName + "-pta.txt");

    if (pName)
        // propagate the resulting name back to the caller
        *pName = plotName;

    // create a dot file
    std::fstream out(fileName.c_str(), std::ios::out);
    if (!out) {
        CL_ERROR("unable to create file '" << fileName << "'");
        return false;
    }

    // open graph
    out << "pta:\n";

    // check whether we can write to stream
    if (!out.flush()) {
        CL_ERROR("unable to write file '" << fileName << "'");
        out.close();
        return false;
    }

    if (loc)
        CL_NOTE_MSG(loc, "writing heap graph to '" << fileName << "'...");
    else
        CL_DEBUG("writing heap graph to '" << fileName << "'...");

    // initialize an instance of PlotData
    PTAData ptadata(sh, out, objs, vals, pHighlight);

    // do our stuff
    extractEverything(ptadata);

    // close graph
    const bool ok = !!out;
    out.close();
    return ok;
}

bool extractPTA(
        const SymHeap                   &sh,
        const std::string               &name,
        const struct cl_loc             *loc)
{
    MyHeapCrawler crawler(sh);

    TObjList allObjs;
    sh.gatherObjects(allObjs);
    for (const TObjId obj : allObjs)
        crawler.digObj(obj);

    const TObjSet objs = crawler.objs();
    const TValSet vals = crawler.vals();

    return extractPTACore(sh, name, loc, objs, vals);
}
