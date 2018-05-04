// Copyright (c) 2015-2017, RAPtor Developer Team
// License: Simplified BSD, http://opensource.org/licenses/BSD-2-Clause
#include "par_matrix.hpp"


using namespace raptor;

bool myFunction (index_t i,index_t j) { return (i<j); }

// proc_to_nodal cerates an on_node matrix from the on_proc matrix
// and part of the off_proc matrix
void ParMatrix::proc_to_nodal() 
{
    // erease after finishing
    int myWorkdRank,mySharedRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myWorkdRank);
    MPI_Comm_rank(partition->topology->local_comm, &mySharedRank);
    //cout << "myWorkdRank: " << myWorkdRank << ", mySharedRank: " << mySharedRank << endl;
    // erease after finishing
    
    const index_t nRows     = on_proc->n_rows; // nRows and nRows_off should be the same
    const index_t nRows_off = off_proc->n_rows;// only if there exist values in off part

    index_t *on_proc_column_map;
    on_proc_column_map = (index_t *) malloc (on_proc_num_cols*sizeof(index_t));

    // recreate the on_proc_column_map
    for (int i=0; i<on_proc_num_cols; ++i) {
        on_proc_column_map[i] =  i + partition->first_local_col;
    } // end for //
    // end of recreating the on_proc_column_map
    
    int firstNodalColumn; 
    int lastNodalColumn;
    
    MPI_Allreduce(&partition->first_local_col, &firstNodalColumn, 1, MPI_INT, MPI_MIN,partition->topology->local_comm);    
    MPI_Allreduce(&partition->last_local_col,  &lastNodalColumn,  1, MPI_INT, MPI_MAX,partition->topology->local_comm);

    index_t nnz = on_proc->nnz;
    index_t nnz_off = off_proc->nnz;

    // expanding the off_proc_map
    int *expandedOffMap = (int *) malloc(off_proc->nnz *sizeof(int ));
    for (int i=0; i< off_proc->nnz; ++i) {
        expandedOffMap[i] = off_proc_column_map[off_proc->idx2[i]];
    } // end for //
    
    
    // estimating the new nnz and nnz_off for the node
    for (int i=0; i<off_proc->nnz ; ++i ) {
        if (expandedOffMap[i] >= firstNodalColumn && expandedOffMap[i] <= lastNodalColumn  )  { 
            ++nnz;
            --nnz_off;
        } // end if //
    } // end for //

    data_t  *val =      (data_t *) malloc(nnz *sizeof(data_t ));
    index_t *row_ptr =  (int *)    calloc((nRows+1),sizeof(int));
    index_t *col_idx =  (int *)    malloc(nnz *sizeof(int));


    data_t  *val_off     = (data_t *) malloc(nnz_off *sizeof(data_t ));
    index_t *row_ptr_off = (int *)    calloc((nRows+1),sizeof(int ));
    index_t *col_idx_off = (int *)    malloc(nnz_off *sizeof(int ));

    
    row_ptr[0] = row_ptr_off[0] = 0;
    int k_node_on ,k_proc_on;
    int k_node_off ,k_proc_off;
    k_node_on = k_proc_on = k_node_off = k_proc_off = 0;

    std::vector<index_t> offNodeMap;

    // transfering non-zeros form original on_proc and off_proc
    // parts to the new on_node in a row by row basis
    for (int i=1; i<=nRows; ++i ) {
        // start by copying the on proc part to on_node part
        int nnzPerRow = on_proc->idx1[i] - on_proc->idx1[i-1];
        row_ptr[i] = row_ptr[i-1] + nnzPerRow;
        for (int j=0; j<nnzPerRow; ++j, ++k_node_on, ++k_proc_on) {
            val[k_node_on]     = on_proc->vals[k_proc_on];
            col_idx[k_node_on] = on_proc_column_map[ (on_proc->idx2[k_proc_on])]-firstNodalColumn;
        } // end for //
        
        // now starts the fun: exploring the off proc part
        row_ptr_off[i] = row_ptr_off[i-1];
        
        if (off_proc->nnz) {
            nnzPerRow = off_proc->idx1[i] - off_proc->idx1[i-1];
            for (int j=0; j<nnzPerRow; ++j, ++k_proc_off) {
            
                if( expandedOffMap[k_proc_off] >= firstNodalColumn && expandedOffMap[k_proc_off] <= lastNodalColumn   ) {
                    // these values will be part of onNode
                    val[k_node_on]     = off_proc->vals[k_proc_off];
                    ++row_ptr[i];
                    col_idx[k_node_on] = expandedOffMap[k_proc_off] - firstNodalColumn;
                    ++k_node_on;
                } else {
                    // these values will be part of offNode
                    val_off[k_node_off]     = off_proc->vals[k_proc_off];
                    ++row_ptr_off[i];
                    //offNodeMap.push_back(A->off_proc_column_map[A->off_proc->idx2[k_proc_off]]);
                    offNodeMap.push_back( expandedOffMap[k_proc_off]);
                    ++k_node_off;
                } // end if //
            } // end for //      
        } // end if //      
        
    } // end for //


    free(on_proc_column_map);
    free(expandedOffMap);
    
    
    
    std::vector<index_t> offNodeMapTemp;
    offNodeMapTemp=offNodeMap;
    
    std::sort (offNodeMap.begin(), offNodeMap.end(), myFunction);
    offNodeMap.erase( unique( offNodeMap.begin(), offNodeMap.end() ), offNodeMap.end() );

    for (int i=0; i<nnz_off; ++i ) {
        int pos = find(offNodeMap.begin(), offNodeMap.end(), offNodeMapTemp[i] ) - offNodeMap.begin();    
        col_idx_off[i] = pos;
    } // end for //



    
    // transfering modified data to actual partition matrix
    
    on_proc->nnz=nnz;
    off_proc->nnz = nnz_off;
    
    for (int i=0; i<=nRows; ++i ) {
        on_proc->idx1[i]=row_ptr[i];
        off_proc->idx1[i]=row_ptr_off[i];
    } // end for //
    
    on_proc->vals.resize(nnz);
    on_proc->idx2.resize(nnz);
    for (int i=0; i<nnz; ++i ) {
        on_proc->vals[i] = val[i];
        on_proc->idx2[i] = col_idx[i];
    }
    
    off_proc->vals.resize(nnz_off);
    off_proc->idx2.resize(nnz_off);
    for (int i=0; i<nnz_off; ++i ) {
        off_proc->vals[i] = val_off[i];
        off_proc->idx2[i] = col_idx_off[i];
    }
    


    free(val);
    free(row_ptr);
    free(col_idx);


    free(val_off);
    free(row_ptr_off);
    free(col_idx_off);

    delete comm;
    comm = new ParComm(partition, offNodeMap);
    off_proc_column_map=offNodeMap;
    off_proc_num_cols = off_proc_column_map.size();
    
}

/*
void ParMatrix::expand_off_col_map() 
{
    expandedOffMap = new int [off_proc->nnz];
    for (int i=0; i< off_proc->nnz; ++i) {
        expandedOffMap[i] = off_proc_column_map[off_proc->idx2[i]];
    } // end for //
}
*/


/**************************************************************
*****   ParMatrix Add Value
**************************************************************
***** Adds a value to the local portion of the parallel matrix,
***** determining whether it should be added to diagonal or 
***** off-diagonal block. 
*****
***** Parameters
***** -------------
***** row : index_t
*****    Local row of value
***** global_col : index_t 
*****    Global column of value
***** value : data_t
*****    Value to be added to parallel matrix
**************************************************************/    
void ParMatrix::add_value(
        int row, 
        index_t global_col, 
        data_t value)
{
    if (global_col >= partition->first_local_col 
            && global_col <= partition->last_local_col)
    {
        on_proc->add_value(row, global_col - partition->first_local_col, value);
    }
    else 
    {
        off_proc->add_value(row, global_col, value);
    }
}

/**************************************************************
*****   ParMatrix Add Global Value
**************************************************************
***** Adds a value to the local portion of the parallel matrix,
***** determining whether it should be added to diagonal or 
***** off-diagonal block. 
*****
***** Parameters
***** -------------
***** global_row : index_t
*****    Global row of value
***** global_col : index_t 
*****    Global column of value
***** value : data_t
*****    Value to be added to parallel matrix
**************************************************************/ 
void ParMatrix::add_global_value(
        index_t global_row, 
        index_t global_col, 
        data_t value)
{
    add_value(global_row - partition->first_local_row, global_col, value);
}

/**************************************************************
*****   ParMatrix Finalize
**************************************************************
***** Finalizes the diagonal and off-diagonal matrices.  Sorts
***** the local_to_global indices, and creates the parallel
***** communicator
*****
***** Parameters
***** -------------
***** create_comm : bool (optional)
*****    Boolean for whether parallel communicator should be 
*****    created (default is true)
**************************************************************/
void ParMatrix::condense_off_proc()
{
    if (off_proc->nnz == 0)
    {
        return;
    }

    int prev_col = -1;

    std::map<int, int> orig_to_new;

    std::copy(off_proc->idx2.begin(), off_proc->idx2.end(),
            std::back_inserter(off_proc_column_map));
    std::sort(off_proc_column_map.begin(), off_proc_column_map.end());

    off_proc_num_cols = 0;
    for (std::vector<int>::iterator it = off_proc_column_map.begin(); 
            it != off_proc_column_map.end(); ++it)
    {
        if (*it != prev_col)
        {
            orig_to_new[*it] = off_proc_num_cols;
            off_proc_column_map[off_proc_num_cols++] = *it;
            prev_col = *it;
        }
    }
    off_proc_column_map.resize(off_proc_num_cols);

    for (std::vector<int>::iterator it = off_proc->idx2.begin();
            it != off_proc->idx2.end(); ++it)
    {
        *it = orig_to_new[*it];
    }
}

// Expands the off_proc_column_map for BSR matrices to hold the
// global columns in off process with non-zeros, not just the
// coarse block columns
void ParMatrix::expand_off_proc(int b_cols)
{
    int start, end;
    std::vector<int> new_map;

    for(int i=0; i<off_proc_column_map.size(); i++)
    {
	start = off_proc_column_map[i] * b_cols;
	if (start >= partition->first_local_col) start += partition->local_num_cols;
	end = start + b_cols;
        for(int j=start; j<end; j++)
	{
            new_map.push_back(j);
	}
    }

    off_proc_column_map.clear();
    std::copy(new_map.begin(), new_map.end(), std::back_inserter(off_proc_column_map));
}

void ParMatrix::finalize(bool create_comm, int b_cols)
{
    on_proc->sort();
    off_proc->sort();

    // Assume nonzeros in each on_proc column
    if (on_proc_num_cols)
    {
        on_proc_column_map.resize(on_proc_num_cols);
        for (int i = 0; i < on_proc_num_cols; i++)
        {
            on_proc_column_map[i] = i + partition->first_local_col;
        }
    }

    if (local_num_rows)
    {
        local_row_map.resize(local_num_rows);
        for (int i = 0; i < local_num_rows; i++)
        {
            local_row_map[i] = i + partition->first_local_row;
        }
    }

    // Condense columns in off_proc, storing global
    // columns as 0-num_cols, and store mapping
    if (off_proc->nnz)
    {
        condense_off_proc();
    }
    else
    {
        off_proc_num_cols = 0;
    }
    off_proc->resize(local_num_rows, off_proc_num_cols);
    local_nnz = on_proc->nnz + off_proc->nnz;

    // If BSR matrix - correct the off_proc_column_map
    if (b_cols) expand_off_proc(b_cols);

    if (create_comm){
        comm = new ParComm(partition, off_proc_column_map);
    }
    else
        comm = new ParComm(partition);
}

int* ParMatrix::map_partition_to_local()
{
    int* on_proc_partition_to_col = new int[partition->local_num_cols+1];
    for (int i = 0; i < partition->local_num_cols+1; i++) on_proc_partition_to_col[i] = -1;
    for (int i = 0; i < on_proc_num_cols; i++)
    {
        on_proc_partition_to_col[on_proc_column_map[i] - partition->first_local_col] = i;
    }

    return on_proc_partition_to_col;
}

void ParMatrix::copy(ParCOOMatrix* A)
{
    if (A->off_proc_num_cols != (int) A->off_proc_column_map.size())
    {
        A->finalize();
    }

    partition = A->partition;
    partition->num_shared++;

    local_nnz = A->local_nnz;
    local_num_rows = A->local_num_rows;
    global_num_rows = A->global_num_rows;
    global_num_cols = A->global_num_cols;

    std::copy(A->off_proc_column_map.begin(), A->off_proc_column_map.end(),
            std::back_inserter(off_proc_column_map));
    std::copy(A->on_proc_column_map.begin(), A->on_proc_column_map.end(),
            std::back_inserter(on_proc_column_map));
    std::copy(A->local_row_map.begin(), A->local_row_map.end(),
            std::back_inserter(local_row_map));

    off_proc_num_cols = off_proc_column_map.size();
    on_proc_num_cols = on_proc_column_map.size();

    if (A->comm)
    {
        comm = new ParComm((ParComm*) A->comm);
    }
    else
    {   
        comm = NULL;
    }
    
    if (A->tap_comm)
    {
        tap_comm = new TAPComm((TAPComm*) A->tap_comm);
    }
    else
    {
        tap_comm = NULL;
    }
}

void ParMatrix::copy(ParCSRMatrix* A)
{
    if (A->off_proc_num_cols != (int) A->off_proc_column_map.size())
    {
        A->finalize();
    }

    partition = A->partition;
    partition->num_shared++;

    local_nnz = A->local_nnz;
    local_num_rows = A->local_num_rows;
    global_num_rows = A->global_num_rows;
    global_num_cols = A->global_num_cols;

    std::copy(A->off_proc_column_map.begin(), A->off_proc_column_map.end(),
            std::back_inserter(off_proc_column_map));
    std::copy(A->on_proc_column_map.begin(), A->on_proc_column_map.end(),
            std::back_inserter(on_proc_column_map));
    std::copy(A->local_row_map.begin(), A->local_row_map.end(),
            std::back_inserter(local_row_map));

    off_proc_num_cols = off_proc_column_map.size();
    on_proc_num_cols = on_proc_column_map.size();

    if (A->comm)
    {
        comm = new ParComm((ParComm*) A->comm);
    }
    else
    {   
        comm = NULL;
    }
    
    if (A->tap_comm)
    {
        tap_comm = new TAPComm((TAPComm*) A->tap_comm);
    }
    else
    {
        tap_comm = NULL;
    }
}

void ParMatrix::copy(ParCSCMatrix* A)
{
    if (A->off_proc_num_cols != (int) A->off_proc_column_map.size())
    {
        A->finalize();
    }

    partition = A->partition;
    partition->num_shared++;

    local_nnz = A->local_nnz;
    local_num_rows = A->local_num_rows;
    global_num_rows = A->global_num_rows;
    global_num_cols = A->global_num_cols;

    std::copy(A->off_proc_column_map.begin(), A->off_proc_column_map.end(),
            std::back_inserter(off_proc_column_map));
    std::copy(A->on_proc_column_map.begin(), A->on_proc_column_map.end(),
            std::back_inserter(on_proc_column_map));
    std::copy(A->local_row_map.begin(), A->local_row_map.end(),
            std::back_inserter(local_row_map));

    off_proc_num_cols = off_proc_column_map.size();
    on_proc_num_cols = on_proc_column_map.size();
    
    if (A->comm)
    {
        comm = new ParComm((ParComm*) A->comm);
    }
    else
    {   
        comm = NULL;
    }
    
    if (A->tap_comm)
    {
        tap_comm = new TAPComm((TAPComm*) A->tap_comm);
    }
    else
    {
        tap_comm = NULL;
    }
}

void ParMatrix::copy(ParBSRMatrix* A)
{
    if (A->off_proc_num_cols != (int) A->off_proc_column_map.size())
    {
        A->finalize();
    }

    partition = A->partition;
    partition->num_shared++;

    local_nnz = A->local_nnz;
    local_num_rows = A->local_num_rows;
    global_num_rows = A->global_num_rows;
    global_num_cols = A->global_num_cols;

    std::copy(A->off_proc_column_map.begin(), A->off_proc_column_map.end(),
            std::back_inserter(off_proc_column_map));
    std::copy(A->on_proc_column_map.begin(), A->on_proc_column_map.end(),
            std::back_inserter(on_proc_column_map));
    std::copy(A->local_row_map.begin(), A->local_row_map.end(),
            std::back_inserter(local_row_map));

    off_proc_num_cols = off_proc_column_map.size();
    on_proc_num_cols = on_proc_column_map.size();

    if (A->comm)
    {
        comm = new ParComm((ParComm*) A->comm);
    }
    else
    {   
        comm = NULL;
    }
    
    if (A->tap_comm)
    {
        tap_comm = new TAPComm((TAPComm*) A->tap_comm);
    }
    else
    {
        tap_comm = NULL;
    }
}

void ParCOOMatrix::copy(ParCSRMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new COOMatrix((CSRMatrix*) A->on_proc);
    off_proc = new COOMatrix((CSRMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCOOMatrix::copy(ParCSCMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new COOMatrix((CSCMatrix*) A->on_proc);
    off_proc = new COOMatrix((CSCMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCOOMatrix::copy(ParCOOMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new COOMatrix((COOMatrix*) A->on_proc);
    off_proc = new COOMatrix((COOMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCOOMatrix::copy(ParBSRMatrix* A)
{
    if (on_proc)
    {
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new COOMatrix((COOMatrix*) A->on_proc);
    off_proc = new COOMatrix((COOMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSRMatrix::copy(ParCSRMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }
    on_proc = new CSRMatrix((CSRMatrix*) A->on_proc);
    off_proc = new CSRMatrix((CSRMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSRMatrix::copy(ParCSCMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }
    on_proc = new CSRMatrix((CSCMatrix*) A->on_proc);
    off_proc = new CSRMatrix((CSCMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSRMatrix::copy(ParCOOMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new CSRMatrix((COOMatrix*) A->on_proc);
    off_proc = new CSRMatrix((COOMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSRMatrix::copy(ParBSRMatrix* A)
{
    printf("Currently not implemented\n");
}

void ParCSCMatrix::copy(ParCSRMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new CSCMatrix((CSRMatrix*) A->on_proc);
    off_proc = new CSCMatrix((CSRMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSCMatrix::copy(ParCSCMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new CSCMatrix((CSCMatrix*) A->on_proc);
    off_proc = new CSCMatrix((CSCMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSCMatrix::copy(ParCOOMatrix* A)
{
    if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new CSCMatrix((COOMatrix*) A->on_proc);
    off_proc = new CSCMatrix((COOMatrix*) A->off_proc);

    ParMatrix::copy(A);
}

void ParCSCMatrix::copy(ParBSRMatrix* A)
{
    printf("Currently not implemented\n");
}

void ParBSRMatrix::copy(ParCSRMatrix* A)
{
    /*if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }
    on_proc = new CSRMatrix((CSRMatrix*) A->on_proc);
    off_proc = new CSRMatrix((CSRMatrix*) A->off_proc);

    ParMatrix::copy(A);*/
    printf("Currently not implemented.\n");
}

void ParBSRMatrix::copy(ParCSCMatrix* A)
{
    /*if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }
    on_proc = new CSRMatrix((CSCMatrix*) A->on_proc);
    off_proc = new CSRMatrix((CSCMatrix*) A->off_proc);

    ParMatrix::copy(A);*/
    printf("Currently not implemented.\n");
}

void ParBSRMatrix::copy(ParCOOMatrix* A)
{
    /*if (on_proc)
    {   
        delete on_proc;
    }
    if (off_proc)
    {
        delete off_proc;
    }

    on_proc = new CSRMatrix((COOMatrix*) A->on_proc);
    off_proc = new CSRMatrix((COOMatrix*) A->off_proc);

    ParMatrix::copy(A);*/
    printf("Currently not implemented.\n");
}

void ParBSRMatrix::copy(ParBSRMatrix* A)
{
    printf("Currently not implemented\n");
}


void ParCOOMatrix::add_block(int global_row_coarse, int global_col_coarse, std::vector<double>& data){
    printf("currently not implemented.\n");
}

void ParCSRMatrix::add_block(int global_row_coarse, int global_col_coarse, std::vector<double>& data){
    printf("currently not implemented.\n");
}

void ParCSCMatrix::add_block(int global_row_coarse, int global_col_coarse, std::vector<double>& data){
    printf("currently not implemented.\n");
}

// THIS FUNCTION NEEDS TO BE TESTED
void ParBSRMatrix::add_block(int global_row_coarse, int global_col_coarse, std::vector<double>& data){

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Takes global row and global column of block in coarse matrix
    if(num_procs <= 1)
    {
        on_proc->add_block(global_row_coarse, global_col_coarse, data);
	local_nnz += b_size;
    }
    else
    {
        // Global indices for block
        int first_row = global_row_coarse * b_rows; // first global row
        int last_row = first_row + b_rows - 1; // last global row
        int first_col = global_col_coarse * b_cols; // first global col
        int last_col = first_col + b_cols - 1; // last global col 

        // local block row index in coarse matrix
        int local_block_row = global_row_coarse % (partition->local_num_rows / b_rows);
        int local_block_col; // local block col index in coarse matrix

        // Check if block belongs to this process - then add to on_proc or off_proc
        if (first_row >= partition->first_local_row &&
            last_row <= partition->last_local_row)
        {
	    //printf("Before add block %d\n", rank);
            if (first_col >= partition->first_local_col &&
                last_col <= partition->last_local_col)
            {
	        local_block_col = global_col_coarse % (partition->local_num_cols / b_cols);
                on_proc->add_block(local_block_row, local_block_col, data);
	    }
	    else
	    {
                // Check to see if block is before on_proc columns or after to 
	        // determine whether local_block_col changes or stays the same
                if (last_col < partition->last_local_col) local_block_col = global_col_coarse;
                else local_block_col = global_col_coarse - (partition->local_num_cols / b_cols);
                off_proc->add_block(local_block_row, local_block_col, data);
	    }

	    //printf("After add block %d\n", rank);
            //printf("rank: %d, fr: %d, lr: %d, fc: %d, lc: %d\n", rank, first_row, last_row, first_col, last_col);
            //printf("rank: %d, lbr: %d, lbc: %d\n", rank, local_block_row, local_block_col);
	    // Update local nnz
	    local_nnz += b_size;
        }
    }
}


ParMatrix* ParCOOMatrix::transpose()
{
    // NOT IMPLEMENTED
    return NULL;
}

ParMatrix* ParCSRMatrix::transpose()
{
    int start, end;
    int proc;
    int col, col_start, col_end;
    int ctr, size;
    int col_count, count;
    int col_size;
    int idx, row;
    MPI_Status recv_status;

    Partition* part_T;
    Matrix* on_proc_T;
    Matrix* off_proc_T;
    CSCMatrix* send_mat;
    CSCMatrix* recv_mat;
    ParCSRMatrix* T = NULL;

    struct PairData 
    {
        double val;
        int index;
    };

    std::vector<PairData> send_buffer;
    std::vector<PairData> recv_buffer;
    std::vector<int> send_ptr(comm->recv_data->num_msgs+1);

    // Transpose partition
    part_T = partition->transpose();

    // Transpose local (on_proc) matrix
    on_proc_T = on_proc->transpose();

    // Allocate vectors for sending off_proc matrix
    send_mat = new CSCMatrix((CSRMatrix*) off_proc);
    recv_mat = new CSCMatrix(local_num_rows, comm->send_data->size_msgs);

    // Add off_proc cols of matrix to send buffer
    ctr = 0;
    send_ptr[0] = 0;
    for (int i = 0; i < comm->recv_data->num_msgs; i++)
    {
        start = comm->recv_data->indptr[i];
        end = comm->recv_data->indptr[i+1];
        for (int j = start; j < end; j++)
        {
            col = j;
            col_start = send_mat->idx1[col];
            col_end = send_mat->idx1[col+1];
            send_buffer.push_back(PairData());
            send_buffer[ctr++].index = col_end - col_start;
            for (int k = col_start; k < col_end; k++)
            {
                send_buffer.push_back(PairData());
                send_buffer[ctr].index = local_row_map[send_mat->idx2[k]];
                send_buffer[ctr++].val = send_mat->vals[k];
            }
        }
        send_ptr[i+1] = send_buffer.size();
    }
    for (int i = 0; i < comm->recv_data->num_msgs; i++)
    {
        proc = comm->recv_data->procs[i];
        start = send_ptr[i];
        end = send_ptr[i+1];
        MPI_Isend(&(send_buffer[start]), end - start, MPI_DOUBLE_INT, proc,
                comm->key, comm->mpi_comm, &(comm->recv_data->requests[i]));
    }
    col_count = 0;
    recv_mat->idx1[0] = 0;
    for (int i = 0; i < comm->send_data->num_msgs; i++)
    {
        proc = comm->send_data->procs[i];
        start = comm->send_data->indptr[i];
        end = comm->send_data->indptr[i+1];
        size = end - start;
        MPI_Probe(proc, comm->key, comm->mpi_comm, &recv_status);
        MPI_Get_count(&recv_status, MPI_DOUBLE_INT, &count);
        if (count > recv_buffer.size())
        {
            recv_buffer.resize(count);
        }
        MPI_Recv(&(recv_buffer[0]), count, MPI_DOUBLE_INT, proc,
                comm->key, comm->mpi_comm, &recv_status);
        ctr = 0;
        for (int j = 0; j < size; j++)
        {
            col_size = recv_buffer[ctr++].index;
            recv_mat->idx1[col_count+1] = recv_mat->idx1[col_count] + col_size;
            col_count++;
            for (int k = 0; k < col_size; k++)
            {
                recv_mat->idx2.push_back(recv_buffer[ctr].index);
                recv_mat->vals.push_back(recv_buffer[ctr++].val);
            }
        }
    }
    recv_mat->nnz = recv_mat->idx2.size();
    MPI_Waitall(comm->recv_data->num_msgs, comm->recv_data->requests.data(), MPI_STATUSES_IGNORE);

    off_proc_T = new CSRMatrix(on_proc_num_cols, -1);
    std::vector<int> off_T_sizes(on_proc_num_cols, 0);
    for (int i = 0; i < comm->send_data->size_msgs; i++)
    {
        row = comm->send_data->indices[i];
        start = recv_mat->idx1[i];
        end = recv_mat->idx1[i+1];
        off_T_sizes[row] += (end - start);
    }
    off_proc_T->idx1[0] = 0;
    for (int i = 0; i < off_proc_T->n_rows; i++)
    {
        off_proc_T->idx1[i+1] = off_proc_T->idx1[i] + off_T_sizes[i];
        off_T_sizes[i] = 0;
    }
    off_proc_T->nnz = off_proc_T->idx1[off_proc_T->n_rows];
    off_proc_T->idx2.resize(off_proc_T->nnz);
    off_proc_T->vals.resize(off_proc_T->nnz);
    for (int i = 0; i < comm->send_data->size_msgs; i++)
    {
        row = comm->send_data->indices[i];
        start = recv_mat->idx1[i];
        end = recv_mat->idx1[i+1];
        for (int j = start; j < end; j++)
        {
            idx = off_proc_T->idx1[row] + off_T_sizes[row]++;
            off_proc_T->idx2[idx] = recv_mat->idx2[j];
            off_proc_T->vals[idx] = recv_mat->vals[j];
        }
    }

    T = new ParCSRMatrix(part_T, on_proc_T, off_proc_T);

    delete send_mat;
    delete recv_mat;

    return T;
}

ParMatrix* ParCSCMatrix::transpose()
{
    // NOT IMPLEMENTED
    return NULL;
}

ParMatrix* ParBSRMatrix::transpose()
{
    // NOT IMPLEMENTED
    return NULL;
}
