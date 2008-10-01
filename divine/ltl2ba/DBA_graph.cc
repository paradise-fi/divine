/*                                                                              
*  class for representation of (semi)deterministic Buchi automaton
*    - implementation of methods
*
*  Milan Ceska - xceska@fi.muni.cz
*
*/

#include <iterator>
#include <stdexcept>

#include "KS_BA_graph.hh"
#include "formul.hh"
#include "DBA_graph.hh"
#include <set>
#include <stack>
#include <algorithm>

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING


DBA_node_t::DBA_node_t(int N, set_pair_t S):KS_BA_node_t(N),sets(S)
{}

DBA_node_t::DBA_node_t(int N,int M):KS_BA_node_t(N),pure_name(M)
{}


DBA_node_t::~DBA_node_t()
{ }


list<KS_BA_node_t*> DBA_node_t::get_adj() const
{
	list<KS_BA_node_t*> L;
	list<DBA_trans_t>::const_iterator t_b, t_e, t_i;

	t_b = adj.begin(); t_e = adj.end();
	for (t_i = t_b; t_i != t_e; t_i++)
        {
	 L.push_back(t_i->target);
	}

	return(L);
}


DBA_graph_t::DBA_graph_t()
{ }

DBA_graph_t::~DBA_graph_t()
{ }

void DBA_graph_t::clear()
{
	map<int, KS_BA_node_t*>::iterator nl_b, nl_e, nl_i;

	nl_b = node_list.begin(); nl_e = node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		delete nl_i->second;
	}

	node_list.erase(nl_b, nl_e);
        set_pair_list.clear();
}


pair<DBA_node_t*,bool> DBA_graph_t::add_node(set_pair_t S)
{
	map<set_pair_t,int>::iterator iter;
        bool new_node=false;
        
        iter=set_pair_list.find(S);
        DBA_node_t *p_N;
        if (iter==set_pair_list.end() )
        {
         timer++;
         new_node = true;
	 p_N = new DBA_node_t(timer,S);
         if ( !(S.second.empty()) ) //co-Buchi -> Buchi 
           (*p_N).accept=true;
         node_list.insert(make_pair(timer,p_N));
         set_pair_list.insert(make_pair(S,timer));
	}
        else
        {
         p_N = dynamic_cast<DBA_node_t*>((*node_list.find( (*iter).second ) ).second);
	}
        return(make_pair(p_N,new_node));
       
}

pair<DBA_node_t*,bool> DBA_graph_t::add_node_s(set_pair_t S)
{
	map<set_pair_t,int>::iterator iter;
        bool new_node=false;
        
        iter=set_pair_list.find(S);
        DBA_node_t *p_N;
        if (iter==set_pair_list.end() )
        {
         timer++;
         new_node = true;
	 p_N = new DBA_node_t(timer,S);
         if ( S.first == S.second ) 
           (*p_N).accept=true;
         node_list.insert(make_pair(timer,p_N));
         set_pair_list.insert(make_pair(S,timer));
	}
        else
        {
         p_N = dynamic_cast<DBA_node_t*>((*node_list.find( (*iter).second ) ).second);
	}
        return(make_pair(p_N,new_node));
       
}




pair<DBA_node_t*,bool> DBA_graph_t::add_pure_node(int N)
{
	map<int,int>::iterator iter;
        bool new_node=false;
        
        iter = pure_state_to_state.find(N);
        DBA_node_t *p_N;
        if (iter==pure_state_to_state.end() )
        {
         timer++;
         new_node = true;
	 p_N = new DBA_node_t(timer,N);
         (*p_N).accept=false;
         node_list.insert(make_pair(timer,p_N));
         pure_state_to_state.insert(make_pair(N,timer));
	}
        else
        {
         p_N = dynamic_cast<DBA_node_t*>((*node_list.find((*iter).second) ).second);
	}
        return(make_pair(p_N,new_node));
       
}




void DBA_graph_t::add_trans(DBA_node_t *p_from, DBA_node_t *p_to, const LTL_label_t& t_label)
{
	DBA_node_t *p_N = (p_from);
	DBA_trans_t Tr;

	Tr.target = (p_to);
	Tr.t_label = t_label;
	p_N->adj.push_back(Tr);
}




list<DBA_trans_t>& DBA_graph_t::get_node_adj(KS_BA_node_t *p_N)
{
	DBA_node_t *p_baN;

	p_baN = dynamic_cast<DBA_node_t*>(p_N);

	if (!p_baN) {
		//cerr << "Nelze prevest uzel" << p_N->name << endl;
		throw runtime_error("Ukazatel neni typu DBA_node_t*");
	}

	return(p_baN->adj);
}


void DBA_graph_t::vypis(ostream& vystup, bool strict_output) const
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<DBA_trans_t>::const_iterator t_b, t_e, t_i;
	LTL_label_t::const_iterator f_b, f_e, f_i;
	const DBA_node_t *p_N;
	bool b1, b2;

	int tr_count = 0, init_count = 0, acc_count = 0;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = dynamic_cast<const DBA_node_t*>(n_i->second);
		vystup << n_i->first;
		if (p_N->initial) {
			vystup << " init";
			init_count++;
		}
		if (p_N->accept) {
			vystup << " accept";
			acc_count++;
		}
		vystup << " : -> {";
		tr_count += p_N->adj.size();
		b1 = false;
		t_b = p_N->adj.begin(); t_e = p_N->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			if (b1) vystup << ", ";
			else b1 = true;
			vystup << "(<";
			f_b = t_i->t_label.begin();
			f_e = t_i->t_label.end();
			b2 = false;
			for (f_i = f_b; f_i != f_e; f_i++) {
				if (b2) vystup << ", ";
				else b2 = true;
				vystup << *f_i;
			}
			vystup << "> - " << t_i->target->name << " = ";
                        set<int> S = t_i->target->sets.first;
                        for(set<int>::iterator i = S.begin(); i!= S.end() ; i++)
                         vystup <<(*i)<<" " ;
                        vystup << " | ";
                        S = t_i->target->sets.second;
                        for(set<int>::iterator i = S.begin(); i!= S.end() ; i++)
                         vystup <<(*i)<<" " ;

                        vystup << ")";
		}
		vystup << "}" << endl;
	}

	if (strict_output) vystup << "# ";
	vystup << "nodes: " << node_list.size() << "; accept: " <<
	acc_count << "; initial: " << init_count << "; transitions: " <<
	tr_count << ";" << endl;
}

void DBA_graph_t::vypis(bool strict_output) const
{
	vypis(cout, strict_output);
}

 KS_BA_node_t* DBA_graph_t::add_node(int name)
 {
  KS_BA_node_t* node;
  return node;
 }
 void DBA_graph_t::transpose(KS_BA_graph_t& Gt) const{}
 void DBA_graph_t::SCC(list<list<int> >& SCC_list){}

/* void DBA_graph_t::determinization(BA_opt_graph_t G)
{
   BA_node_t *node;
   LTL_label_t label;
   DBA_node_t *Dnode_p,*new_Dnode_p;
   list<KS_BA_node_t*> accepts,initials,nodes;
   set<int> targets,S1,S2;
   set_pair_t set_pair,pom_pair;
   pair<DBA_node_t*,bool> pair;
   map<LTL_label_t,set<int> > labels_targets,labels_targets2;
   map<LTL_label_t,set<int> >::iterator labels_targets_iter,labels_targets2_iter;
   stack<int> node_stack;
   
   initials = G.get_init_nodes();//mel by byt jen jeden
   nodes = G.get_all_nodes();
   accepts.clear();
   
   for(list<KS_BA_node_t*>::iterator k= nodes.begin(); k != nodes.end();k++)
     if ( !((*(*k)).accept) ) accepts.push_back((*k)); // Buch -> co-Buchi 
   
   timer=1; // hack kvuli stejnemu cislovani
   
   node =  dynamic_cast<BA_node_t*>(initials.front());
   pom_pair.first.clear();
   pom_pair.second.clear();
   pom_pair.first.insert((*node).name);
   
   
   Dnode_p = new DBA_node_t(timer,pom_pair);
   (*Dnode_p).initial=true;
   node_list.insert(make_pair(timer,Dnode_p));
   set_pair_list.insert(make_pair(pom_pair,timer));
   node_stack.push((*Dnode_p).name);

   while( !(node_stack.empty()) )
   {
     Dnode_p =dynamic_cast<DBA_node_t*>( (*node_list.find(node_stack.top())).second) ;
     node_stack.pop();
     pom_pair = (*Dnode_p).sets;
     //R=emptyset/
     if (pom_pair.second.empty())
     {
       labels_targets.clear();   
       for(set<int>::iterator i=pom_pair.first.begin(); i!= pom_pair.first.end();i++)
       {
          list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second))).adj;
	  list<BA_trans_t> complete_trans = Complete(trans);
          for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
          {
            node = (*j).target;
            label = (*j).t_label;
            labels_targets_iter = labels_targets.find(label);
            if (labels_targets_iter == labels_targets.end())
             {
              targets.clear();
              targets.insert((*node).name) ;
              labels_targets.insert(make_pair(label,targets));
             } 
            else{
             targets=((*labels_targets_iter).second);
             targets.insert((*node).name) ;
             labels_targets[label]=targets;
            }
          }
       }
       for(labels_targets_iter =labels_targets.begin(); labels_targets_iter!=labels_targets.end();
           labels_targets_iter++)
       {
         S1=((*labels_targets_iter).second);
         S2=S1;    
         for(list<KS_BA_node_t*>::iterator k=accepts.begin(); k!=accepts.end();k++)
           S2.erase((*(*k)).name);
         set_pair=make_pair(S1,S2);
         pair = add_node(set_pair);
         if (pair.second )
         {
          new_Dnode_p = pair.first;
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
          node_stack.push((*new_Dnode_p).name);
         }
         else{
          new_Dnode_p = pair.first;
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
         }
       }
     }
      //R!=emptyset
     else{
       labels_targets.clear(); 
       labels_targets2.clear();  
       for(set<int>::iterator i=pom_pair.first.begin(); i!= pom_pair.first.end();i++)
       {
          list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second))).adj;
	  list<BA_trans_t> complete_trans = Complete(trans);
          for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
          {
            node = (*j).target;
            label = (*j).t_label;
            labels_targets_iter = labels_targets.find(label);
            if (labels_targets_iter == labels_targets.end()) 
             {
              targets.clear();
              targets.insert((*node).name) ;
              labels_targets.insert(make_pair(label,targets));
              if (pom_pair.second.find(*i)!= pom_pair.second.end())
               { 
                  labels_targets2.insert(make_pair(label,targets));
                  //for(set<int>::iterator k = targets.begin(); k != targets.end(); k++)
                  // cout<<"vkladam pro "<<label.front() <<" "<<(*k) <<endl;
                  //cout<<"konec"<<endl;
               }
             } 
            else{
             targets=((*labels_targets_iter).second);
             targets.insert((*node).name) ;
             labels_targets[label]=targets;
             if (pom_pair.second.find(*i)!= pom_pair.second.end())
             {
               labels_targets2_iter = labels_targets2.find(label);
               if (labels_targets2_iter == labels_targets2.end()) 
               {
                 targets.clear();
                 targets.insert((*node).name); 
                 labels_targets2.insert(make_pair(label,targets));
	         //for(set<int>::iterator l = targets.begin(); l != targets.end(); l++)
                  // cout<<"vkladam 2 pro "<<label.front() <<" "<<(*l) <<endl;
                  //cout<<"konec"<<endl;
               }
               else{
               targets=((*labels_targets2_iter).second);
               targets.insert((*node).name) ;
               labels_targets2[label]=targets;
               //for(set<int>::iterator l = targets.begin(); l != targets.end(); l++)
               //cout<<"vkladam 3 pro "<<label.front() <<" "<<(*l) <<endl;
               //cout<<"konec"<<endl;
               }
             }
            }
          }
          
       }
       
       for(labels_targets_iter =labels_targets.begin(); labels_targets_iter!=labels_targets.end();
           labels_targets_iter++)
       {
         S1=((*labels_targets_iter).second);
         label = (*labels_targets_iter).first;
         labels_targets2_iter = labels_targets2.find(label);
         if(labels_targets2_iter==labels_targets2.end() ) {S2.clear();}
         else{ 
           S2=((*labels_targets2_iter).second);    
           for(list<KS_BA_node_t*>::iterator k=accepts.begin(); k!=accepts.end();k++) 
             S2.erase((*(*k)).name);
         }
         set_pair=make_pair(S1,S2);
         pair = add_node(set_pair);
         if (pair.second )
         {
          new_Dnode_p = pair.first;
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
          node_stack.push((*new_Dnode_p).name);
         }
         else{
          new_Dnode_p = pair.first;
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
         }
       
       }
     }
   } 
}*/

void DBA_graph_t::semideterminization(BA_opt_graph_t G)
{
   BA_node_t *node,*node_p;
   LTL_label_t label;
   DBA_node_t *Dnode_p,*Dnode_s,*new_Dnode_p;
   list<KS_BA_node_t*>initials;
   list<DBA_node_t*>root_nodes;
   set<int> targets,S1,S2,S3;
   set_pair_t set_pair,pom_pair;
   pair<DBA_node_t*,bool> pair;
   list<BA_trans_t> trans ;
   map<LTL_label_t,set<int> > labels_targets,labels_targets2,labels_targets3;
   map<LTL_label_t,set<int> >::iterator labels_targets_iter,labels_targets2_iter,labels_targets3_iter;
   stack<int> node_stack;
   
   initials = G.get_init_nodes();//mel by byt jen jeden
   root_nodes.clear();
   
   timer=1; // hack kvuli stejnemu cislovani
   
   node =  dynamic_cast<BA_node_t*>(initials.front());
   
   Dnode_p = new DBA_node_t(timer,(*node).name);
   (*Dnode_p).initial=true;
   if ( (*node).accept )  root_nodes.push_back(Dnode_p); 

   node_list.insert(make_pair(timer,Dnode_p));
   pure_state_to_state.insert(make_pair((*node).name,timer));
   node_stack.push(timer);


   //cout<< " prvni cast automatu"<<endl;
   while( !(node_stack.empty()) )
   {
     Dnode_p =dynamic_cast<DBA_node_t*>( (*node_list.find(node_stack.top())).second) ;
     node_stack.pop();         
     list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*Dnode_p).pure_name) )).second))).adj;
     for(list<BA_trans_t>::iterator j=trans.begin();j!=trans.end();j++)
      {
        node = (*j).target;
        label = (*j).t_label;
        pair = add_pure_node((*node).name);
     
         if (pair.second )
         {
          new_Dnode_p = pair.first;
          if ( (*node).accept )  root_nodes.push_back(new_Dnode_p);
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>(label) );
          node_stack.push((*new_Dnode_p).name);
         }
         else{
          new_Dnode_p = pair.first;
          add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>(label) );
         }
       }
    }
     //cout<<"druha cast automatu"<<endl;
    for( list<DBA_node_t*>::iterator root_iter = root_nodes.begin(); root_iter != root_nodes.end(); root_iter++)
    {
       Dnode_s = (*root_iter);
       //pom_pair.first.clear();
       //pom_pair.second.clear();
       //pom_pair.first.insert((*Dnode_s).pure_name);
       //pom_pair.second.insert((*Dnode_s).pure_name);

       //timer++;
       //Dnode_p = new DBA_node_t(timer,pom_pair);
       //node_list.insert(make_pair(timer,Dnode_p));
       //set_pair_list.insert(make_pair(pom_pair,timer));
       labels_targets.clear();
       trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*Dnode_s).pure_name) )).second))).adj;
       list<BA_trans_t> complete_trans = Complete(trans);
       for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
       {    
         node = (*j).target;
         label = (*j).t_label;
         labels_targets_iter = labels_targets.find(label);
          if (labels_targets_iter == labels_targets.end())
            {
             targets.clear();
             targets.insert((*node).name) ;
             labels_targets.insert(make_pair(label,targets));
            } 
           else{
            targets=((*labels_targets_iter).second);
            targets.insert((*node).name) ;
            labels_targets[label]=targets;
           }
       }
       for(labels_targets_iter =labels_targets.begin(); labels_targets_iter!=labels_targets.end();
           labels_targets_iter++)
       {
         S1=((*labels_targets_iter).second);
         set_pair=make_pair(S1,S1);//jen u rootu
         pair = add_node_s(set_pair);
         if (pair.second )
         {
          new_Dnode_p = pair.first;
          //add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
          add_trans(Dnode_s, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
          node_stack.push((*new_Dnode_p).name);
         }
         else{
          new_Dnode_p = pair.first;
          //add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
          add_trans(Dnode_s, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
         }    
       }
    }
    //cout<<"konec for pres rooty"<<endl;
    while( !(node_stack.empty()) )
       {
        Dnode_p =dynamic_cast<DBA_node_t*>( (*node_list.find(node_stack.top())).second) ;
        node_stack.pop();
        pom_pair = (*Dnode_p).sets;
        
        if (pom_pair.second == pom_pair.first)//P=G
         {
          labels_targets.clear();   
          for(set<int>::iterator i=pom_pair.first.begin(); i!= pom_pair.first.end();i++)
          {
           list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second))).adj;
	   list<BA_trans_t> complete_trans = Complete(trans);
           for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
            {
             node = (*j).target;
             label = (*j).t_label;
             labels_targets_iter = labels_targets.find(label);
             if (labels_targets_iter == labels_targets.end())
              {
               targets.clear();
               targets.insert((*node).name) ;
               labels_targets.insert(make_pair(label,targets));
              }  
             else{
              targets=((*labels_targets_iter).second);
              targets.insert((*node).name) ;
              labels_targets[label]=targets;
             }
            }
          }
          
          labels_targets2.clear(); 
          for(set<int>::iterator i=pom_pair.first.begin(); i!= pom_pair.first.end();i++)
          {
           node_p = dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second));
           if( (*node_p).accept )
           {
             list<BA_trans_t> trans =(*node_p).adj;
	     list<BA_trans_t> complete_trans = Complete(trans);
             for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
             {
              node = (*j).target;
              label = (*j).t_label;
              labels_targets2_iter = labels_targets2.find(label);
              if (labels_targets2_iter == labels_targets2.end())
              {
               targets.clear();
               targets.insert((*node).name) ;
               labels_targets2.insert(make_pair(label,targets));
              }  
              else{
               targets=((*labels_targets2_iter).second);
               targets.insert((*node).name) ;
               labels_targets2[label]=targets;
              }
             }
            } 
          }
	  for(labels_targets_iter =labels_targets.begin(); labels_targets_iter!=labels_targets.end();
	      labels_targets_iter++)
          {
           S2=((*labels_targets_iter).second);
           labels_targets2_iter = labels_targets2.find((*labels_targets_iter).first);
           if ( labels_targets2_iter != labels_targets2.end() )
            S1=(*labels_targets2_iter).second;
           else
            S1.clear();

           set_pair=make_pair(S1,S2);
           pair = add_node_s(set_pair);
           if (pair.second )
           {
            new_Dnode_p = pair.first;
            add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
            node_stack.push((*new_Dnode_p).name);
           }
           else{
            new_Dnode_p = pair.first;
            add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
           }
          }
         }//konec then
         //P!=G
         else {
          labels_targets.clear();   
          for(set<int>::iterator i=pom_pair.second.begin(); i!= pom_pair.second.end();i++)
          {
           list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second))).adj;
	   list<BA_trans_t> complete_trans = Complete(trans);
           for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
            {
             node = (*j).target;
             label = (*j).t_label;
             labels_targets_iter = labels_targets.find(label);
             if (labels_targets_iter == labels_targets.end())
              {
               targets.clear();
               targets.insert((*node).name) ;
               labels_targets.insert(make_pair(label,targets));
              }  
             else{
              targets=((*labels_targets_iter).second);
              targets.insert((*node).name) ;
              labels_targets[label]=targets;
             }
            }
          }
          labels_targets3.clear(); 
          for(set<int>::iterator i=pom_pair.second.begin(); i!= pom_pair.second.end();i++)
          {
           node_p = dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second));
           if( (*node_p).accept )
           {
             list<BA_trans_t> trans =(*node_p).adj;
	     list<BA_trans_t> complete_trans = Complete(trans);
             for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
             {
              node = (*j).target;
              label = (*j).t_label;
              labels_targets3_iter = labels_targets3.find(label);
              if (labels_targets3_iter == labels_targets3.end())
              {
               targets.clear();
               targets.insert((*node).name) ;
               labels_targets3.insert(make_pair(label,targets));
              }  
              else{
               targets=((*labels_targets3_iter).second);
               targets.insert((*node).name) ;
               labels_targets3[label]=targets;
              }
             }
            } 
          }

	  labels_targets2.clear();   
          for(set<int>::iterator i=pom_pair.first.begin(); i!= pom_pair.first.end();i++)
          {
           list<BA_trans_t> trans =(*dynamic_cast<BA_node_t*>(((*(G.node_list.find((*i)) )).second))).adj;
	   list<BA_trans_t> complete_trans = Complete(trans);
           for(list<BA_trans_t>::iterator j=complete_trans.begin();j!=complete_trans.end();j++)
            {
             node = (*j).target;
             label = (*j).t_label;
             labels_targets2_iter = labels_targets2.find(label);
             if (labels_targets2_iter == labels_targets2.end())
              {
               targets.clear();
               targets.insert((*node).name) ;
               labels_targets2.insert(make_pair(label,targets));
              }  
             else{
              targets=((*labels_targets2_iter).second);
              targets.insert((*node).name) ;
              labels_targets2[label]=targets;
             }
            }
          }
	  for(labels_targets_iter =labels_targets.begin(); labels_targets_iter!=labels_targets.end();
	      labels_targets_iter++)
          {
           S2=((*labels_targets_iter).second);
           
           labels_targets2_iter = labels_targets2.find((*labels_targets_iter).first);
           if ( labels_targets2_iter != labels_targets2.end() )
            S1=(*labels_targets2_iter).second;
           else
            S1.clear();
           
           labels_targets3_iter = labels_targets3.find((*labels_targets_iter).first);
           if ( labels_targets3_iter != labels_targets3.end() )
            S3=(*labels_targets3_iter).second;
           else
            S3.clear();           

           set<int> S4;
           S4.clear();
           for(set<int>::iterator S_iter = S1.begin();S_iter != S1.end();S_iter++)
             S4.insert((*S_iter));
           for(set<int>::iterator S_iter= S3.begin();S_iter != S3.end();S_iter++)
             S4.insert((*S_iter));

           set_pair=make_pair(S4,S2);
           pair = add_node_s(set_pair);
           if (pair.second )
           {
            new_Dnode_p = pair.first;
            add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
            node_stack.push((*new_Dnode_p).name);
           }
           else{
            new_Dnode_p = pair.first;
            add_trans(Dnode_p, new_Dnode_p,dynamic_cast<const LTL_label_t&>((*labels_targets_iter).first) );
           }
          }
         }//konec else
    }//konec while 

}

BA_opt_graph_t DBA_graph_t::transform()
{
 BA_opt_graph_t G;
 node_list_t::const_iterator n_b, n_e, n_i;
 list<DBA_trans_t>::const_iterator t_b, t_e, t_i;
 KS_BA_node_t *p_N;

 n_b = node_list.begin(); n_e = node_list.end();
 for (n_i = n_b; n_i != n_e; n_i++)
 {
  p_N = (G.add_node(n_i->first));
  p_N->initial = n_i->second->initial;
  p_N->accept = n_i->second->accept;
  t_b = get_node_adj(n_i->second).begin();
  t_e = get_node_adj(n_i->second).end();
  for (t_i = t_b; t_i != t_e; t_i++) 
   G.add_trans(p_N, t_i->target->name, t_i->t_label);
 
 }
  return G;
}

list<BA_trans_t> DBA_graph_t::Complete(list<BA_trans_t> trans)
 {
   list<BA_trans_t> complete_trans;
   list<LTL_label_t> old_labels,new_labels ;
   LTL_label_t all_literal;
   bool found = false;
   LTL_label_t::iterator all_literal_iter;
   for(list<BA_trans_t>::iterator i=trans.begin(); i!= trans.end();i++)
   {
    for(list<LTL_literal_t>::iterator j = (*i).t_label.begin();  j != (*i).t_label.end();j++)
    {
      for(list<LTL_literal_t>::iterator all_literal_iter = all_literal.begin();all_literal_iter != all_literal.end();all_literal_iter++)
       if ( (*all_literal_iter).predikat == (*j).predikat ) found = true;
      if (!found) all_literal.push_back((*j));      
    }
   }
   for(list<BA_trans_t>::iterator i=trans.begin(); i!= trans.end();i++)
   {
    list<LTL_literal_t> base = (*i).t_label;
    old_labels.clear();
    new_labels.clear();
    old_labels.push_back((*i).t_label);
    for(list<LTL_literal_t>::iterator all_literal_iter = all_literal.begin();all_literal_iter != all_literal.end();all_literal_iter++) 
    {
     found = false;
     for(list<LTL_literal_t>::iterator base_iter = base.begin(); base_iter != base.end();base_iter++)
      if ( (*base_iter).predikat == (*all_literal_iter).predikat ) found = true; 
     
     if (!found) //neni v bazi
      {
        LTL_literal_t new_literal_1, new_literal_2;
        new_literal_1.predikat = (*all_literal_iter).predikat;
        new_literal_1.negace = (*all_literal_iter).negace;
        new_literal_2.predikat = (*all_literal_iter).predikat;
        new_literal_2.negace = !((*all_literal_iter).negace);
        for(list<LTL_label_t>::iterator old_labels_iter = old_labels.begin(); old_labels_iter != old_labels.end(); old_labels_iter++)
         {
            LTL_label_t pom_1 = (*old_labels_iter);
 	    LTL_label_t pom_2 = (*old_labels_iter);
            pom_1.push_back(new_literal_1);
            pom_2.push_back(new_literal_2);
            new_labels.push_back(pom_1);
            new_labels.push_back(pom_2);
         }         
        old_labels = new_labels;
        
      }
    }
    BA_trans_t new_trans;
    for(list<LTL_label_t>::iterator old_labels_iter = old_labels.begin(); old_labels_iter != old_labels.end(); old_labels_iter++)
      {
        new_trans.t_label =  (*old_labels_iter);
        new_trans.target = (*i).target;
        complete_trans.push_back(new_trans); 
      }
   }
  return complete_trans;
 }
