/*
 * Copyright (C) 2011 Jiri Simacek
 *
 * This file is part of forester.
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

#ifndef SPLITTING_H
#define SPLITTING_H

#include <vector>
#include <set>

#include "forestautext.hh"
#include "folding.hh"
#include "programerror.hh"
#include "utils.hh"

struct RootEnumF {

	size_t target;
	std::set<size_t>& selectors;

	RootEnumF(size_t target, std::set<size_t>& selectors)
		: target(target), selectors(selectors) {}

	bool operator()(const AbstractBox* aBox, size_t, size_t) {
		if (!aBox->isStructural())
			return true;
		const StructuralBox* sBox = (const StructuralBox*)aBox;
		this->selectors.insert(sBox->outputCoverage().begin(), sBox->outputCoverage().end());
		return true;
	}

};

struct LeafEnumF {

	const FAE& fae;
	const TT<label_type>& t;
	size_t target;
	std::set<size_t>& selectors;

	LeafEnumF(const FAE& fae, const TT<label_type>& t, size_t target, std::set<size_t>& selectors)
		: fae(fae), t(t), target(target), selectors(selectors) {}

	bool operator()(const AbstractBox* aBox, size_t, size_t offset) {
		if (!aBox->isType(box_type_e::bBox))
			return true;
		const Box* box = (const Box*)aBox;
		for (size_t k = 0; k < box->getArity(); ++k, ++offset) {
			size_t ref;
			if (fae.getRef(this->t.lhs()[offset], ref) && ref == this->target)
				this->selectors.insert(box->inputCoverage(k).begin(), box->inputCoverage(k).end());
		}
		return true;
	}

};

struct LeafScanF {

	const FAE& fae;
	const TT<label_type>& t;
	size_t selector;
	size_t target;
	const Box*& matched;

	LeafScanF(const FAE& fae, const TT<label_type>& t, size_t selector, size_t target, const Box*& matched)
		: fae(fae), t(t), selector(selector), target(target), matched(matched) {}

	bool operator()(const AbstractBox* aBox, size_t, size_t offset) {
		if (!aBox->isType(box_type_e::bBox))
			return true;
		const Box* box = (const Box*)aBox;
		for (size_t k = 0; k < box->getArity(); ++k, ++offset) {
			size_t ref;
			if (fae.getRef(this->t.lhs()[offset], ref) && ref == this->target && box->inputCovers(k, this->selector)) {
				this->matched = box;
				return false;
			}
		}
		return true;
	}

};

struct IsolateOneF {
	size_t offset;

	IsolateOneF(size_t offset) : offset(offset) {}

	bool operator()(const StructuralBox* box) const {
		return box->outputCovers(this->offset);
	}
};

struct IsolateBoxF {
	const Box* box;

	IsolateBoxF(const Box* box) : box(box) {}

	bool operator()(const StructuralBox* box) const {
		return this->box == box;
	}
};

struct IsolateSetF {
	std::set<size_t> s;
	
	IsolateSetF(const std::vector<size_t>& v, size_t offset = 0) {
		for (std::vector<size_t>::const_iterator i = v.begin(); i != v.end(); ++i)
			this->s.insert(*i + offset);
	}
	
	bool operator()(const StructuralBox* box) const {
		return utils::checkIntersection(box->outputCoverage(), this->s);
	}
};

struct IsolateAllF {
	bool operator()(const StructuralBox*) const {
		return true;
	}
};

class Splitting {

	FAE& fae;

public:

	// enumerates downwards selectors
	void enumerateSelectorsAtRoot(std::set<size_t>& selectors, size_t target) const {
		assert(target < this->fae.roots.size());
		assert(this->fae.roots[target]);
		this->fae.roots[target]->begin(
			*this->fae.roots[target]->getFinalStates().begin()
		)->label()->iterate(RootEnumF(target, selectors));		
	}

	// enumerates upwards selectors
	void enumerateSelectorsAtLeaf(std::set<size_t>& selectors, size_t root, size_t target) const {
		assert(root < this->fae.roots.size());
		assert(this->fae.roots[root]);

		for (TA<label_type>::iterator i = this->fae.roots[root]->begin(); i != this->fae.roots[root]->end(); ++i) {
			if (i->label()->isNode())
				i->label()->iterate(LeafEnumF(this->fae, *i, target, selectors));
		}
	}

	// enumerates upwards selectors
	void enumerateSelectorsAtLeaf(std::set<size_t>& selectors, size_t target) const {
		for (size_t root = 0; root < this->fae.roots.size(); ++root) {
			if (!this->fae.roots[root])
				continue;
			for (TA<label_type>::iterator i = this->fae.roots[root]->begin(); i != this->fae.roots[root]->end(); ++i) {
				if (i->label()->isNode())
					i->label()->iterate(LeafEnumF(this->fae, *i, target, selectors));
			}
		}
	}


	void enumerateSelectors(std::set<size_t>& selectors, size_t target) const {
		assert(target < this->fae.roots.size());
		assert(this->fae.roots[target]);
		this->enumerateSelectorsAtRoot(selectors, target);
		this->enumerateSelectorsAtLeaf(selectors, target);
	}
	
	struct CheckIntegrityF {

		const Splitting& split;
		const TA<label_type>& ta;
		const TT<label_type>& t;
		std::set<size_t>* required;
		std::vector<bool>& bitmap;
		std::map<std::pair<const TA<label_type>*, size_t>, std::set<size_t>>& states;
	
		CheckIntegrityF(const Splitting& split, const TA<label_type>& ta, const TT<label_type>& t,
			std::set<size_t>* required, std::vector<bool>& bitmap, std::map<std::pair<const TA<label_type>*, size_t>, std::set<size_t>>& states)
			: split(split), ta(ta), t(t), required(required), bitmap(bitmap), states(states) {
		}
	
		bool operator()(const AbstractBox* aBox, size_t, size_t offset) {
			switch (aBox->getType()){
				case bBox: {
					const Box* tmp = (const Box*)aBox;
					for (size_t i = 0; i < tmp->getArity(); ++i)
						this->split.checkState(this->ta, this->t.lhs()[offset + i], tmp->inputCoverage(i), this->bitmap, states);
					break;
				}
				case bSel:
					this->split.checkState(this->ta, this->t.lhs()[offset], std::set<size_t>(), this->bitmap, states);
					break;
				default:
					break;
			}

			if (this->required && aBox->isStructural()) {
				for (auto s : ((const StructuralBox*)aBox)->outputCoverage()) {
					auto c = this->required->erase(s);
					assert(c == 1);
				}
			}

			return true;
			
		}
	
	};

	void checkState(const TA<label_type>& ta, size_t state, const std::set<size_t>& defined,
		std::vector<bool>& bitmap, std::map<std::pair<const TA<label_type>*, size_t>, std::set<size_t>>& states) const {

		const Data* data;

		if (this->fae.isData(state, data)) {

			if (data->isRef())
				this->checkRoot(data->d_ref.root, bitmap, states);

			return;

		}

		auto p = states.insert(std::make_pair(std::make_pair(&ta, state), defined));

		if (!p.second) {
			assert(defined == p.first->second);
			return;
		}

		for (TA<label_type>::iterator i = ta.begin(state); i != ta.end(state); ++i) {

			TypeBox* typeBox = (TypeBox*)i->label()->boxLookup((size_t)(-1), NULL);
/*
			if (!typeBox) {
				
				i->label()->iterate(CheckIntegrityF(*this, ta, *i, NULL, bitmap, states));

				continue;

			}
*/
			const std::vector<size_t>& sels = typeBox->getSelectors();
	
			std::set<size_t> tmp(sels.begin(), sels.end());
	
			for (auto s : defined) {
				
				auto c = tmp.erase(s);
	
				assert(c == 1);
	
			}
			
			i->label()->iterate(CheckIntegrityF(*this, ta, *i, &tmp, bitmap, states));
	
			assert(tmp.empty());
	
		}
		
	}

	void checkRoot(size_t root, std::vector<bool>& bitmap, std::map<std::pair<const TA<label_type>*, size_t>, std::set<size_t>>& states) const {

		assert(this->fae.roots[root]);
		
		if (bitmap[root])
			return;

		bitmap[root] = true;

		std::set<size_t> tmp;

		this->enumerateSelectorsAtLeaf(tmp, root);

		for (auto s : this->fae.roots[root]->getFinalStates())
			this->checkState(*this->fae.roots[root], s, tmp, bitmap, states);

	}

	bool checkIntegrity() const {

		std::vector<bool> bitmap(this->fae.roots.size(), false);
		std::map<std::pair<const TA<label_type>*, size_t>, std::set<size_t>> states;

		for (size_t i = 0; i < this->fae.roots.size(); ++i) {

			if (!this->fae.roots[i])
				continue;

			this->checkRoot(i, bitmap, states);

		}

		return true;;

	}

	// adds redundant root points to allow further manipulation
	void isolateAtLeaf(std::vector<FAE*>& dst, size_t root, size_t target, size_t selector) const {

//		CL_CDEBUG(3, std::endl << this->fae);
//		CL_CDEBUG(3, "isolateAtLeaf: (" << root << ", " << selector << ':' << target << ')');

		assert(root < this->fae.roots.size());
		assert(this->fae.roots[root]);

		this->fae.unreachableFree(this->fae.roots[root]);

		std::vector<std::pair<const TT<label_type>*, const Box*> > v;

		TA<label_type> ta(*this->fae.backend);

		const Box* matched;
		for (TA<label_type>::iterator i = this->fae.roots[root]->begin(); i != this->fae.roots[root]->end(); ++i) {
			if (!i->label()->isNode()) {
				ta.addTransition(*i);
				continue;
			}
			matched = NULL;
			i->label()->iterate(LeafScanF(this->fae, *i, selector, target, matched));
			if (matched) {
				v.push_back(std::make_pair(&*i, matched));
			} else {
				ta.addTransition(*i);
			}
		}

		assert(v.size());

		for (std::vector<std::pair<const TT<label_type>*, const Box*> >::iterator i = v.begin(); i != v.end(); ++i) {
			FAE fae(this->fae);
			Splitting splitting(fae);
			TA<label_type> ta2(*fae.backend);
			if (this->fae.roots[root]->isFinalState(i->first->rhs())) {
				ta.copyTransitions(ta2);
				size_t state = fae.freshState();
				ta2.addFinalState(state);
				const TT<label_type>& t = ta2.addTransition(i->first->lhs(), i->first->label(), state)->first;
				fae.roots[root] = std::shared_ptr<TA<label_type>>(&ta2.uselessAndUnreachableFree(*fae.allocTA()));
				fae.updateRootMap(root);
				std::set<const Box*> boxes;
				splitting.isolateAtRoot(root, t, IsolateBoxF(i->second), boxes);
				assert(boxes.count(i->second));
				Folding(fae).unfoldBox(root, i->second);
				splitting.isolateOne(dst, target, selector);
				continue;
			}
			ta2.addFinalStates(this->fae.roots[root]->getFinalStates());
			for (TA<label_type>::iterator j = ta.begin(); j != ta.end(); ++j) {
				ta2.addTransition(*j);
				std::vector<size_t> lhs = j->lhs();
				for (std::vector<size_t>::iterator k = lhs.begin(); k != lhs.end(); ++k) {
					if (*k == i->first->rhs()) {
						*k = fae.addData(ta2, Data::createRef(fae.roots.size()));
						ta2.addTransition(lhs, j->label(), j->rhs());
						*k = i->first->rhs();
					}
				}
			}
			fae.roots[root] = std::shared_ptr<TA<label_type>>(&ta2.uselessAndUnreachableFree(*fae.allocTA()));
			fae.updateRootMap(root);
			ta2.clear();
			size_t state = fae.freshState();
			ta2.addFinalState(state);
			const TT<label_type>& t = ta2.addTransition(i->first->lhs(), i->first->label(), state)->first;
			ta.copyTransitions(ta2);
			fae.appendRoot(&ta2.uselessAndUnreachableFree(*fae.allocTA()));
			fae.rootMap.push_back(std::vector<std::pair<size_t, bool> >());
			std::set<const Box*> boxes;
			splitting.isolateAtRoot(fae.roots.size() - 1, t, IsolateBoxF(i->second), boxes);
			assert(boxes.count(i->second));
			Folding(fae).unfoldBox(fae.roots.size() - 1, i->second);
			splitting.isolateOne(dst, target, selector);
		}

	}

	// adds redundant root points to allow further manipulation
	template <class F>
	void isolateAtRoot(size_t root, const TT<label_type>& t, F f, std::set<const Box*>& boxes) {
		assert(root < this->fae.roots.size());
		assert(this->fae.roots[root]);
		size_t newState = this->fae.freshState();
		TA<label_type> ta(*this->fae.roots[root], false);
		ta.addFinalState(newState);
		std::vector<size_t> lhs;
		size_t lhsOffset = 0;
		for (std::vector<const AbstractBox*>::const_iterator j = t.label()->getNode().begin(); j != t.label()->getNode().end(); ++j) {
			if (!(*j)->isStructural())
				continue;
			StructuralBox* b = (StructuralBox*)(*j);
			if (!f(b)) {
				// this box is not interesting
				for (size_t k = 0; k < (*j)->getArity(); ++k, ++lhsOffset)
					lhs.push_back(t.lhs()[lhsOffset]);
				continue;
			}
			// we have to isolate here
			for (size_t k = 0; k < (*j)->getArity(); ++k, ++lhsOffset) {
				if (FA::isData(t.lhs()[lhsOffset])) {
					// no need to create a leaf when it's already there
					lhs.push_back(t.lhs()[lhsOffset]);
					continue;
				}
				// update new left-hand side
				lhs.push_back(this->fae.addData(ta, Data::createRef(this->fae.roots.size())));
				// prepare new root
				TA<label_type> tmp(*this->fae.roots[root], false);
				tmp.addFinalState(t.lhs()[lhsOffset]);
				TA<label_type>* tmp2 = this->fae.allocTA();
				tmp.unreachableFree(*tmp2);
				// update 'o'
				this->fae.appendRoot(tmp2);
				this->fae.rootMap.push_back(std::vector<std::pair<size_t, bool> >());
				this->fae.updateRootMap(this->fae.roots.size() - 1);
			}
			if (b->isType(box_type_e::bBox))
				boxes.insert((const Box*)*j);
		}
		ta.addTransition(lhs, t.label(), newState);
		TA<label_type>* tmp = this->fae.allocTA();
		ta.unreachableFree(*tmp);
		// exchange the original automaton with the new one
		this->fae.roots[root] = std::shared_ptr<TA<label_type>>(tmp);
		this->fae.updateRootMap(root);
	}

	// adds redundant root points to allow further manipulation
	template <class F>
	void isolateAtRoot(std::vector<FAE*>& dst, size_t root, F f) const {

		assert(root < this->fae.roots.size());
		assert(this->fae.roots[root]);

		CL_CDEBUG(3, "isolateAtRoot: " << root << std::endl << this->fae);

		for (std::set<size_t>::const_iterator j = this->fae.roots[root]->getFinalStates().begin(); j != this->fae.roots[root]->getFinalStates().end(); ++j) {
		for (TA<label_type>::iterator i = this->fae.roots[root]->begin(*j), end = this->fae.roots[root]->end(*j, i); i != end ; ++i) {
			FAE fae(this->fae);
			Splitting splitting(fae);
			std::set<const Box*> boxes;
			splitting.isolateAtRoot(root, *i, f, boxes);
//			std::cerr << "after isolation: " << std::endl << fae;
			if (!boxes.empty()) {
				Folding(fae).unfoldBoxes(root, boxes);
//				std::cerr << "after decomposition: " << std::endl << fae;
				splitting.isolateAtRoot(dst, root, f);
			} else {
				dst.push_back(new FAE(fae));
			}
		}
		}

	}

	void isolateOne(std::vector<FAE*>& dst, size_t target, size_t offset) const {

//		CL_CDEBUG(3, "isolateOne: " << target << ':' << offset << std::endl << this->fae);

		assert(target < this->fae.roots.size());
		assert(this->fae.roots[target]);

		std::set<size_t> tmp;

		this->enumerateSelectorsAtRoot(tmp, target);

		if (tmp.count(offset)) {
			this->isolateAtRoot(dst, target, IsolateOneF(offset));
			return;
		}

		for (size_t i = 0; i < this->fae.roots.size(); ++i) {
			tmp.clear();
			for (std::vector<std::pair<size_t, bool> >::const_iterator j = this->fae.rootMap[i].begin(); j != this->fae.rootMap[i].end(); ++j) {
				if (j->first != target)
					continue;
				this->enumerateSelectorsAtLeaf(tmp, i, target);
				if (tmp.count(offset)) {
					this->isolateAtLeaf(dst, i, target, offset);
					return;
				}
				break;					
			}
		}

		throw ProgramError("isolateOne(): selector lookup failed!");

	}

	void isolateSet(std::vector<FAE*>& dst, size_t target, int base, const std::vector<size_t>& offsets) const {

		assert(target < this->fae.roots.size());
		assert(this->fae.roots[target]);

//		CL_CDEBUG(3, "isolateSet: " << target << ", " << base << " + " << utils::wrap(offsets));

		std::vector<size_t> offsD;
		std::set<size_t> tmpS, offsU;

		this->enumerateSelectorsAtRoot(tmpS, target);

		for (std::vector<size_t>::const_iterator i = offsets.begin(); i != offsets.end(); ++i) {
			if (tmpS.count(base + *i))
				offsD.push_back(base + *i);
			else
				offsU.insert(base + *i);
		}

//		CL_CDEBUG(3, "offsD: " << utils::wrap(offsD));
//		CL_CDEBUG(3, "offsU: " << utils::wrap(offsU));

		if (offsU.empty()) {
			this->isolateAtRoot(dst, target, IsolateSetF(offsD));
			return;
		}

		std::vector<FAE*> tmp, tmp2;

		ContainerGuard<std::vector<FAE*> > g(tmp), f(tmp2);

		if (offsD.size())
			this->isolateAtRoot(tmp, target, IsolateSetF(offsD));
		else
			tmp.push_back(new FAE(this->fae));

		for (std::set<size_t>::iterator i = offsU.begin(); i != offsU.end(); ++i) {
			for (std::vector<FAE*>::iterator j = tmp.begin(); j != tmp.end(); ++j) {
				tmpS.clear();			
				Splitting splitting(**j);
				splitting.enumerateSelectorsAtRoot(tmpS, target);
				if (tmpS.count(*i))
					tmp2.push_back(new FAE(**j));
				else {
					bool found = false;
					for (size_t k = 0; k < (*j)->roots.size(); ++k) {
						if (!(*j)->roots[k] || !(*j)->hasReference(k, target))
							continue;
						tmpS.clear();
						splitting.enumerateSelectorsAtLeaf(tmpS, k, target);
						if (tmpS.count(*i)) {
							splitting.isolateAtLeaf(tmp2, k, target, *i);
							found = true;
							break;
						}						
					}
					if (!found)
						throw ProgramError("isolateSet(): selector lookup failed!");
				}
			}
			utils::erase(tmp);
			std::swap(tmp, tmp2);
		}

		assert(tmp.size());

		dst.insert(dst.end(), tmp.begin(), tmp.end());

		g.release();
		f.release();

	}

	void restrictedSplit(Index<size_t>& index, size_t root, size_t state) {
		TA<label_type> ta(*this->fae.backend);
		this->fae.roots[root]->copyTransitions(ta);
		ta.addFinalState(state);
		TA<label_type> ta2(*this->fae.backend);
		this->fae.roots[root]->copyTransitions(ta2);
		index.set(state, this->fae.addData(ta2, Data::createRef(this->fae.roots.size())));
		size_t base = this->fae.nextState();
		bool changed = true;
		while (changed) {
			changed = false;
			for (TA<label_type>::iterator i = ta2.begin(); i != ta2.end(); ++i) {
				std::vector<size_t> lhs = i->lhs();
				for (size_t j = 0; j < lhs.size(); ++j) {
					std::pair<size_t, bool> p = index.find(lhs[j]);
					if (p.second) {
						lhs[j] = p.first;
						ta2.addTransition(lhs, i->label(), index.get(i->rhs(), base));
						lhs[j] = i->lhs()[j];
					}
				}
			}
		}
		this->fae.incrementStateOffset(index.size());
		const std::set<size_t>& s = this->fae.roots[root]->getFinalStates();
		for (std::set<size_t>::const_iterator i = s.begin(); i != s.end(); ++i) {
			std::pair<size_t, bool> p = index.find(*i);
			if (p.second)
				ta2.addFinalState(p.first);			
		}
		// update FAE
		TA<label_type>* tmp = this->fae.allocTA();
		ta.unreachableFree(*tmp);
		this->fae.appendRoot(tmp);
		this->fae.rootMap.push_back(std::vector<std::pair<size_t, bool> >());
		this->fae.updateRootMap(this->fae.roots.size() - 1);
		tmp = this->fae.allocTA();
		ta2.unreachableFree(*tmp);
		this->fae.roots[root] = std::shared_ptr<TA<label_type>>(tmp);
		this->fae.updateRootMap(root);
	}

public:

	Splitting(FAE& fae) : fae(fae) {}
	Splitting(const FAE& fae) : fae(*(FAE*)&fae) {}

};

#endif
