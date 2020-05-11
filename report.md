# Report


## Choose parent

**Init**    
At the begining, Node have rank 255.  A broadcast is made to discover parent. If a node have a rank, it reply (with is rank).  Current node have `parent_rank+1`.    

**Change parent**    
When a node have a rank, it can change his parent. To do that, two conditions are required:   
- better rank (so: 'current_rank > new_parent_rank') (notice: does it work if they have the same rank ?)
- better quality signal than the parent (based on last quality signal meseare with the parent)


