/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2016 INRIA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*! \file SiconosSet.hpp
  Template class to define a set of Siconos object.

*/
#ifndef SiconosSet_H
#define SiconosSet_H

#include <set>
#include <map>
#include <iostream>
#include "Cmp.hpp"
#include "RuntimeException.hpp"

/** Set (STL) of pointers to T -
 *
 *  \author SICONOS Development Team - copyright INRIA
 *  \version 3.0.0.
 *  \date (Creation) May 17, 2006
 *
 * A SiconosSet<T,U> handles a set of pointers to T, sorted in growing \n
 * order, according to the value (of type U) returned by a member \n
 * function "getSort" of class T.\n
 * Thus: T object must have a function named getSort:
 * \code
 * const U getSort() const
 * \endcode
 * "<" being defined for type-U objects. \n
 * See for example Interaction.h or DynamicalSystem.h
 *
 * Possible operations are insert, erase, get or check presence of T, \n
 * intersection or difference of sets (in a mathematical sense).
 *
 */
template <class T, class U> class SiconosSet
{
public:

  /** set of T */
  typedef std::set<std11::shared_ptr<T>, Cmp<T, U> > TSet;

  /** iterator through a set of T */
  typedef typename TSet::iterator TIterator;

  /** const iterator through a set of T */
  typedef typename TSet::const_iterator ConstTIterator;

  /** return type value for insert function - bool = false if insertion failed. */
  typedef typename std::pair<TIterator, bool> CheckInsertT;

protected:
  /** serialization hooks
  */
  typedef void serializable;
  template<typename Archive>
  friend void siconos_io(Archive&, SiconosSet<T, U>&, const unsigned int);
  friend class boost::serialization::access;


  /** Pointer to function used in ordering relation */
  U(T::*fpt)() const;

  /** a set of T, sorted thanks to their address */
  std11::shared_ptr<TSet> setOfT;

private:

  /** copy constructor, private => forbidden
   *  \param a SiconosSet to be copied
   */
  SiconosSet(const SiconosSet&);

public:

  /** default constructor
   */
  SiconosSet(): fpt(NULL)
  {
    fpt = &T::getSort;
    setOfT.reset(new TSet(fpt));
  };

  ~SiconosSet()
  {
    clear();
  };

  /** return the number of Ts in the set
   *  \return an unsigned int
   */
  unsigned int size() const
  {
    return setOfT->size();
  };

  /** return true if the set is empty, else false
   *  \return a bool
   */
  bool isEmpty() const
  {
    return setOfT->empty();
  };

  /** iterator equal to the first element of setOfT
   *  \return a TIterator
   */
  TIterator begin()
  {
    return setOfT->begin();
  };

  /** iterator equal to setOfT->end()
   *  \return a TIterator
   */
  TIterator end()
  {
    return setOfT->end();
  }

  /** const iterator equal to the first element of setOfT
   *  \return a TIterator
   */
  ConstTIterator begin() const
  {
    return setOfT->begin();
  };

  /** const iterator equal to setOfT->end()
   *  \return a TIterator
   */
  ConstTIterator end() const
  {
    return setOfT->end();
  }

  /** return setOfT
   *  \return an InterSet
   */
  const std11::shared_ptr<TSet> getSet() const
  {
    return setOfT;
  }

  /** get T number num, if it is present in the set (else, exception)
   *  \return a pointer to T
   */
  std11::shared_ptr<T> getPtr(int num) const
  {
    ConstTIterator it;
    for (it = setOfT->begin(); it != setOfT->end(); ++it)
    {
      if (((*it)->number()) == num)
        break;
    }
    if (it == setOfT->end())
      RuntimeException::selfThrow("SiconosSet::get(num): can not find an object number ""num"" in the set.");
    return *it;
  };

  /** return true if ds is in the set
   *  \param a pointer to T
   *  \return a bool
   */
  bool isIn(std11::shared_ptr<T> t) const
  {
    TIterator it = setOfT->find(t);
    bool out = false;
    if (it != setOfT->end()) out = true;
    return out;
  };

  /** return true if T number num is in the set
   *  \param an int
   *  \return a bool
   */
  bool isIn(int num) const
  {
    bool out = false;
    TIterator it;
    for (it = setOfT->begin(); it != setOfT->end(); ++it)
    {
      if (((*it)->number()) == num)
      {
        out = true;
        break;
      }
    }
    return out;
  }


  /** same as find function of stl set
   *  \param a pointer to T
   *  \param a TIterator
   */
  TIterator find(std11::shared_ptr<T> t)
  {
    return setOfT->find(t);
  };

  /** same as find function of stl set
   *  \param an int
   *  \return a TIterator
   */
  TIterator find(int num)
  {
    TIterator it;
    for (it = setOfT->begin(); it != setOfT->end(); ++it)
    {
      if (((*it)->number()) == num)
        break;
    }
    return it; // == this.end() if not found.
  };

  /** insert a T* into the set
   *  \param a pointer to T
   *  \return a CheckInsertT (boolean type information)
   */
  CheckInsertT insert(std11::shared_ptr<T> t)
  {
    return setOfT->insert(t);
  };

  /** insert a range into the set
      \param iterator to the first element to be inserted
      \param iterator after the last element to be inserted
  */
  template <class InputIterator>
  void insert(InputIterator begin, InputIterator end)
  {
    setOfT->insert(begin, end);
  }

  /** remove a T* from the set
   *  \param a pointer to T
   */
  void erase(std11::shared_ptr<T> t)
  {
    TIterator it = setOfT->find(t);
    if (it == setOfT->end())
      RuntimeException::selfThrow("SiconosSet::erase(t): t is not in the set!");

    setOfT->erase(*it);
  };

  /** remove all Ts from the set
   */
  void clear()
  {
    setOfT->clear();
  };

  /** screen-display of the numbers of the Ts present in the set.
   */
  void display() const
  {
    std::cout << "====> Set display - The following objects are present in the set (number):" << std::endl;
    TIterator it;
    for (it = setOfT->begin(); it != setOfT->end(); ++it)
      std::cout << "(" << (*it)->number() << "), ";
    std::cout << std::endl;
    std::cout << "=============================================================================================" << std::endl;
  };

  /**   computes in s3 the intersection of s1 and s2 (-> set_intersection stl function) */
  /*       \param SiconosSet, s1 */
  /*       \param SiconosSet, s2 */
  /*       \param SiconosSet, s3 */
  friend void intersection(const SiconosSet& s1, const SiconosSet& s2, SiconosSet& commonT)
  {
    set_intersection(s1.setOfT->begin(), s1.setOfT->end(), s2.setOfT->begin(), s2.setOfT->end(),
                     inserter(*commonT.setOfT, commonT.setOfT->begin()), (commonT.setOfT)->value_comp());
  };

  /** computes in s3 the difference betwee s1 and s2 (-> set_difference stl function)
      \param SiconosSet, s1
      \param SiconosSet, s2
      \param SiconosSet, s3
  */
  friend void difference(const SiconosSet& s1, const SiconosSet& s2, SiconosSet& commonT)
  {
    set_difference(s1.setOfT->begin(), s1.setOfT->end(), s2.setOfT->begin(), s2.setOfT->end(),
                   inserter(*commonT.setOfT, commonT.setOfT->begin()), (commonT.setOfT)->value_comp());
  };
};

#endif


