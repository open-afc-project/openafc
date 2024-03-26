/******************************************************************************************/
/**** FILE: list.h                                                                     ****/
/******************************************************************************************/

#ifndef LIST_H
#define LIST_H

#include <functional>

/******************************************************************************************/
/**** Template Class: ListClass                                                        ****/
/**** Based on template class example from "C++ Inside & Out", pg 528.                 ****/
/******************************************************************************************/
template<class T>
class ListClass
{
	public:
		ListClass(int n, int incr = 10);
		~ListClass();
		T &operator[](int index) const;
		int operator==(ListClass<T> &val);
		int getSize() const;
		void append(T val);
		void reset();
		void resize(int n = 0);
		void reverse();
		void del_elem(T val, const int err = 1);
		void del_elem_idx(int index, const int err = 1);
		void toggle_elem(T val);
		T pop(); // push not needed because append does the same thing.

		int ins_elem(T val, const int err = 1, int *insFlagPtr = (int *)0);
		int get_index(T val, const int err = 1) const;
		int contains(T val) const;

		void sort(const std::function<bool(const T &, const T &)> &compare);

		void printlist(int n = 100,
			       const char *elemSep = (const char *)0,
			       const char *grpSep = (const char *)0,
			       const char *endStr = (const char *)0);

	private:
		T *a;
		int size;
		int allocation_size;
		int allocation_increment;
};
/******************************************************************************************/

template<class T>
int get_index_sorted(ListClass<T> *lc_t, T val, int err = 1);

template<class T>
int contains_sorted(ListClass<T> *lc_t, T val);

template<class T>
int ins_elem_sorted(ListClass<T> *lc_t, T val, const int err = 1, int *insFlagPtr = (int *)0);

template<class T>
void sort(ListClass<T> *lc_t);

template<class T, class U>
void sort2(ListClass<T> *lc_t, ListClass<U> *lc_u);

template<class T, class U, class V>
void sort3(ListClass<T> *lc_t, ListClass<U> *lc_u, ListClass<V> *lc_v);

template<class T>
int ins_pointer(ListClass<T> *lc_t, T &val, int err = 1, int *insFlagPtr = (int *)0);

template<class T>
void sort_pointer(ListClass<T> *lc_t);

template<class T>
int contains_pointer(ListClass<T> *lc_t, T &val);

template<class T>
void readOneCol(const char *filename, ListClass<T> *lc_t);

template<class T, class U>
T crossList(const ListClass<T> *lc_t, const U *ilist, U val, int err = 1);

#endif
