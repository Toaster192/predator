/*
 * Copyright (C) 2009 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of sl.
 *
 * sl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * sl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sl.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef H_GUARD_STORAGE_H
#define H_GUARD_STORAGE_H

#include "code_listener.h"

#include <map>
#include <string>
#include <vector>

#ifndef BUILDING_DOX
#   define STD_VECTOR(type) std::vector<type>
#else
#   define STD_VECTOR(type) type*
#endif

/**
 * @file storage.hh
 * an easy to use model of storage for serialised code_listener objects
 */

/**
 * an easy to use model of storage for serialised code_listener objects
 */
namespace CodeStorage {

/**
 * high-level variable (operand) classification
 */
enum EVar {
    VAR_VOID,           ///< this should be used only internally
    VAR_GL,             ///< global (may be also static) variable
    VAR_LC,             ///< local variable (not valid beyond a function)
    VAR_FNC_ARG,        ///< fnc argument (sort of local variable also)
    VAR_REG             ///< intermediate code register (kind of local variable)
};

/**
 * high-level variable representation
 */
struct Var {
    EVar                        code;   ///< high-level type of variable
    struct cl_location          loc;    ///< location of its declaration

    /**
     * type of the variable
     * @note This often differs from type of the operand given to constructor!
     */
    const struct cl_type        *clt;

    /**
     * unique ID of variable
     * @attention not guaranteed to be unique beyond the scope of variable
     */
    int                         uid;

    /**
     * name of the variable, empty string for anonymous variables, e.g.\ VAR_REG
     */
    std::string                 name;

    /**
     * dummy constructor
     * @note known to be useful for internal purposes only
     */
    Var();
    ~Var();

    /**
     * wrap low-level operand to Var object
     * @param code type of variable
     * @param op Pointer to low-level operand to be used for Var construction.
     */
    Var(EVar code, const struct cl_operand *op);
};

/**
 * lookup container for set of Var objects
 */
class VarDb {
    private:
        typedef STD_VECTOR(Var) TList;

    public:
        typedef TList::const_iterator const_iterator;
        typedef const_iterator iterator;

    public:
        VarDb();
        ~VarDb();
        VarDb(const VarDb &);               ///< shallow copy
        VarDb& operator=(const VarDb &);    ///< shallow copy

        /**
         * look for a Var object by ID, add one if not found
         * @param uid ID of variable to look for
         * @return reference to either a found or just created Var object
         */
        Var& operator[](int uid);

        /**
         * look for a Var object by ID, crash if not found
         * @attention It is not safe to look for non-existing ID, it will jump
         * to debugger in that case.
         * @param uid ID of variable to look for
         * @return reference to the found Var object
         */
        const Var& operator[](int uid) const;

        /**
         * return STL-like iterator to go through the container
         */
        const_iterator begin() const { return vars_.begin(); }

        /**
         * return STL-like iterator to go through the container
         */
        const_iterator end()   const { return vars_.end();   }

        /**
         * return count of object stored in the container
         */
        size_t size()          const { return vars_.size();  }

    private:
        TList vars_;
        struct Private;
        Private *d;
};

/**
 * type lookup table
 * @attention Type objects are not cloned for now (sort of exception).
 * @todo Check if there is a front-end which really needs types to be cloned.
 */
class TypeDb {
    public:
        TypeDb();
        ~TypeDb();

        /**
         * index given type for lookup
         * @note useful only for builder
         * @return true if the type was really added, false if it has been
         * already there
         */
        bool insert(const struct cl_type *);

        /**
         * type lookup by ID
         * @attention The pointer returned is the same as formerly given to
         * insert(). There is no cloning performed since it hasn't been
         * considered useful for now.
         */
        const struct cl_type* operator[](int) const;

    private:
        /// @b not allowed to be copied
        TypeDb(const TypeDb &);

        /// @b not allowed to be copied
        TypeDb& operator=(const TypeDb &);

    private:
        struct Private;
        Private *d;
};

/**
 * Add the given type into TypeDb instance, then descent into the type and add
 * all the referred types recursively.
 * @param db TypeDb object to add all types to
 * @param clt an arbitrary code listener type to be read
 */
void readTypeTree(TypeDb &db, const struct cl_type *clt);

class Block;
class ControlFlow;

/**
 * generic STL-based list of cl_operand objects.
 * They may or may not be deep-cloned, it depends on the particular purpose.
 */
typedef STD_VECTOR(struct cl_operand) TOperandList;

/**
 * generic STL-based list of Block pointers (useful to build CFG from Block
 * objects)
 */
typedef STD_VECTOR(const Block *) TTargetList;

/**
 * high-level representation of an intermediate code instruction
 */
struct Insn {
    /**
     * type of instruction, see #cl_insn_e documentation
     * @note now there can be also CL_INSN_CALL and CL_INSN_SWITCH
     */
    enum cl_insn_e              code;

    /**
     * some extra instructions partitioning, for now used by CL_INSN_UNOP and
     * CL_INSN_BINOP
     * @note this suffers from a little type info lost since it represent both
     * enumeral types #cl_unop_e and #cl_binop_e by an integer.
     */
    int                         subCode;

    /**
     * corresponding location in the original source code
     */
    struct cl_location          loc;

    /**
     * List of all operands used by the instruction.
     * Their particular semantic is highly dependent on @b type @b of @b the
     * @b instruction. Let's go to summarize it:
     *
     * - (@b 0 operands) @b CL_INSN_JMP
     *
     * - (@b 1 operands) @b CL_INSN_COND:
     *      - [@b 0] - value to branch by
     *
     * - (@b 1 operands) @b CL_INSN_RET:
     *      - [@b 0] - value to return from fnc
     *
     * - (@b 0 operands) @b CL_INSN_ABORT
     *
     * - (@b 2 operands) @b CL_INSN_UNOP:
     *      - [@b 0] - destination
     *      - [@b 1] - source
     *
     * - (@b 3 operands) @b CL_INSN_BINOP:
     *      - [@b 0] - destination
     *      - [@b 1] - source 1
     *      - [@b 2] - source 2
     *
     * - (@b 2+ operands) @b CL_INSN_CALL:
     *      - [@b 0] - destination
     *      - [@b 1] - what are we going to call (mostly a fnc)
     *      - [@b 2..@b n] - call arguments
     *
     * - (@b 1+ operands) @b CL_INSN_SWITCH:
     *      - [@b 0] - value to switch by
     *      - [@b 1..@b n] - case values
     */
    TOperandList                operands;

    /**
     * List of all target blocks - useful only for @b terminal @b instructions.
     * Their particular semantic is highly dependent on @b type @b of @b the
     * @b instruction. Let's go to summarize it:
     *
     * - (@b 1 target) @b CL_INSN_JMP:
     *      - [@b 0] - target of jump
     *
     * - (@b 2 targets) @b CL_INSN_COND:
     *      - [@b 0] - where to jump if condition holds
     *      - [@b 1] - where to jump if condition does not hold
     *
     * - (@b 0 targets) @b CL_INSN_RET
     *
     * - (@b 0 targets) @b CL_INSN_ABORT
     *
     * - (@b 1+ targets) @b CL_INSN_SWITCH:
     *      - [@b 0] - where to jump by default
     *      - [@b 1..@b n] - where to jump in corresponding cases
     */
    TTargetList                 targets;
};

/**
 * Basic block - a single node in ControlFlow graph. Once the basic block is
 * ready, it contains (possibly empty) sequence of non-terminating instructions
 * and exactly one terminating instruction.
 * @todo reverse links to precedent blocks?
 */
class Block {
    private:
        typedef STD_VECTOR(const Insn *) TList;

    public:
        typedef TList::const_iterator const_iterator;
        typedef const_iterator iterator;

    public:
        /**
         * constructor useful to place objects into std::vector, but do @b not
         * try to call append() on objects constructed this way. It would crash
         * on a NULL pointer dereference.
         */
        Block():
            cfg_(0)
        {
        }

        /**
         * created a named basic block
         * @param cfg pointer to control flow graph where the block belongs to
         * @param name name of the basic block being constructed (zero ended C
         * string)
         */
        Block(ControlFlow *cfg, const char *name):
            cfg_(cfg),
            name_(name)
        {
        }
        
        // NOTE: there is no destructor ... given objects are NOT destroyed

        /**
         * return name of the basic block
         */
        const std::string& name() const {
            return name_;
        }

        /**
         * append a given instruction to and of the block
         * @param insn Instruction to append.
         * @note Given objects are not cloned nor destroyed!
         */
        void append(const Insn *insn);

        /**
         * return list of all directly successor basic blocks
         */
        const TTargetList& targets() const;

        /**
         * return STL-like iterator to go through all the instructions inside
         */
        const_iterator begin()               const { return insns_.begin(); }

        /**
         * return STL-like iterator to go through all the instructions inside
         */
        const_iterator end()                 const { return insns_.end();   }

        /**
         * return count of instructions inside the basic block
         */
        size_t size()                        const { return insns_.size();  }

        /**
         * direct access to instruction by its index.
         * @param idx Index of instruction to access (staring with zero).
         * @attention There is no range check performed.
         */
        const Insn* operator[](unsigned idx) const { return insns_[idx];    }

    private:
        TList insns_;
        ControlFlow *cfg_;
        std::string name_;
};

/**
 * Control flow graph - an easy to analyse representation of the intermediate
 * code. Nodes of the graph are basic blocks - instances of Block
 * @todo list of exits?
 * @todo list of aborts?
 * @todo support for (non-recursive) call graph unfolding
 */
class ControlFlow {
    private:
        typedef STD_VECTOR(Block *) TList;

    public:
        typedef TList::const_iterator const_iterator;
        typedef const_iterator iterator;

    public:
        ControlFlow();
        ~ControlFlow();
        ControlFlow(const ControlFlow &);               ///< shallow copy
        ControlFlow& operator=(const ControlFlow &);    ///< shallow copy

        /**
         * return entry basic block
         */
        const Block *entry() const;

        /**
         * look for a basic block by name, create one if not found
         * @param name name of the basic block to look for
         * @return referenced pointer to either a found or just created Block
         * @attention created objects will @b not be destroyed automatically
         */
        Block*& operator[](const char *name);

        /**
         * look for a basic block by name, crash if not found
         * @attention It is not safe to look for a non-existing basic block, it
         * will jump to debugger in that case.
         * @param name name of the basic block to look for
         * @return pointer to the found Var object
         */
        const Block* operator[](const char *name) const;

        /**
         * return STL-like iterator to go through all basic blocks inside
         */
        const_iterator begin()                const { return bbs_.begin(); }

        /**
         * return STL-like iterator to go through all basic blocks inside
         */
        const_iterator end()                  const { return bbs_.end();   }

        /**
         * return count of basic blocks inside the control flow graph
         */
        size_t size()                         const { return bbs_.size();  }

    private:
        TList bbs_;
        struct Private;
        Private *d;
};

struct File;

/**
 * function definition
 */
struct Fnc {
    File                        *file;  ///< reference place of fnc definition
    struct cl_operand           def;    ///< definition as low-level operand
    VarDb                       vars;   ///< per-function local variables
    ControlFlow                 cfg;    ///< fnc code as control flow graph

    Fnc():
        file(0)
    {
        def.code = CL_OPERAND_VOID;
    }
};

/**
 * return the name of given Fnc object (if any)
 */
const char* nameOf(const Fnc &);

/**
 * return the uid of given Fnc object
 */
int uidOf(const Fnc &);

/**
 * lookup container for set of Fnc objects
 */
class FncDb {
    private:
        typedef STD_VECTOR(Fnc *) TList;

    public:
        typedef TList::const_iterator const_iterator;
        typedef const_iterator iterator;

    public:
        FncDb();
        ~FncDb();
        FncDb(const FncDb &);                       ///< shallow copy
        FncDb& operator=(const FncDb &);            ///< shallow copy

        /**
         * look for a function by ID, create one if not found
         * @param uid ID of the function to look for
         * @return referenced pointer to either a found or just created Fnc obj
         * @attention created objects will @b not be destroyed automatically
         */
        Fnc*& operator[](int uid);

        /**
         * look for a function by ID, crash if not found
         * @attention It is not safe to look for a non-existing function, it
         * will jump to debugger in that case.
         * @param uid ID of the function to look for
         * @return pointer to the found Fnc object
         */
        const Fnc* operator[](int uid) const;

        /**
         * return STL-like iterator to go through all functions inside
         */
        const_iterator begin() const { return fncs_.begin(); }

        /**
         * return STL-like iterator to go through all functions inside
         */
        const_iterator end()   const { return fncs_.end();   }

        /**
         * return count of functions inside the container
         */
        size_t size()          const { return fncs_.size();  }

    private:
        TList fncs_;
        struct Private;
        Private *d;
};

/**
 * STL-based map from function name to corresponding Fnc object
 */
typedef std::map<std::string, Fnc *> TFncNames;

/**
 * file content representation
 */
struct File {
    /* const */ std::string     name;           ///< file name
    VarDb                       vars;           ///< static variables
    FncDb                       fncs;           ///< functions per file
    TFncNames                   fncByName;      ///< associated fnc names

    File(const std::string &name_):
        name(name_)
    {
    }
};

/**
 * lookup container for set of Fnc objects
 */
class FileDb {
    private:
        typedef STD_VECTOR(File *) TList;

    public:
        typedef TList::const_iterator const_iterator;
        typedef const_iterator iterator;

    public:
        FileDb();
        ~FileDb();
        FileDb(const FileDb &);                 ///< shallow copy
        FileDb& operator=(const FileDb &);      ///< shallow copy

        /**
         * look for a File object by name, create one if not found
         * @param name name of the file to look for
         * @return referenced pointer to either a found or just created File
         * @note given file name is always canonicalized before lookup
         * @attention created objects will @b not be destroyed automatically
         */
        File*& operator[](const char *name);

        /**
         * look for a File object by name, crash if not found
         * @attention It is not safe to look for a non-existing File object, it
         * will jump to debugger in that case.
         * @param name name of the file to look for
         * @return pointer to the found File object
         * @note given file name is always canonicalized before lookup
         */
        const File* operator[](const char *name) const;

        /**
         * return STL-like iterator to go through all files inside
         */
        const_iterator begin() const { return files_.begin(); }

        /**
         * return STL-like iterator to go through all files inside
         */
        const_iterator end()   const { return files_.end();   }

        /**
         * return count of files inside the container
         */
        size_t size()          const { return files_.size();  }

    private:
        TList files_;
        struct Private;
        Private *d;
};

/**
 * a value type representing the @b whole @b serialised @b model of code
 * @todo Consider adding all-in-one VarDb. Note this will need globally unique
 * IDs for all vars. Well, we have something like that while using gcc. But
 * isn't the requirement too restrictive for other parsers?
 */
struct Storage {
    TypeDb                      types;              ///< type info access point
    VarDb                       glVars;             ///< global variables
    TFncNames                   glFncByName;        ///< names of gl fncs
    FileDb                      files;              ///< per file content
    FncDb                       orphans;            ///< @note not used for now
};

} // namespace CodeStorage

#endif /* H_GUARD_STORAGE_H */
