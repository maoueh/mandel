#pragma once
#include <eosio/chain/types.hpp>
#include <eosio/chain/block_state.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/trace.hpp>
#include <functional>
#include <tuple>

namespace eosio::chain {

class signals_processor {
public:
   using trx_deque = eosio::chain::deque< std::tuple< chain::transaction_trace_ptr, packed_transaction_ptr > >;
   using applied_transaction_bs_func = std::function< void ( const trx_deque&, const chain::block_state_ptr& ) >;
   using irreversible_block_func = std::function< void ( const chain::block_state_ptr& ) >;
   using block_start_func = std::function< void ( uint32_t block_num ) >;
   using accepted_block_func = std::function< void ( const chain::block_state_ptr& ) >;
   using applied_transaction_func = std::function< void ( const chain::transaction_trace_ptr&, const packed_transaction_ptr& ) >;

   /**
    * Class for tracking transactions and which block they belong to.
    */
   signals_processor(bool alternate_interface = false)
   : _alternate_interface(alternate_interface) {
   }

   void register_callbacks(applied_transaction_bs_func atbs, irreversible_block_func ib, block_start_func bs, accepted_block_func ab, applied_transaction_func at) {
      _callbacks.emplace_back(atbs, ib, bs, ab, at);
   }

   /// connect to chain controller applied_transaction signal
   void signal_applied_transaction( const chain::transaction_trace_ptr& trace, const chain::packed_transaction_ptr& ptrx ) {
      if (_alternate_interface) {
         for(auto& callback : _callbacks) {
            try {
               std::get<4>(callback)(trace, ptrx);
            } FC_LOG_AND_DROP(("Failed to pass applied transaction to callback"));
         }
      }
      else {
         _trxs.emplace_back(trace, ptrx);
      }
   }

   /// connect to chain controller accepted_block signal
   void signal_accepted_block( const chain::block_state_ptr& bsp ) {
      if (_alternate_interface) {
         for(auto& callback : _callbacks) {
            try {
               std::get<3>(callback)(bsp);
            } FC_LOG_AND_DROP(("Failed to pass accepted block to callback"));
         }
      }
      else {
         push_transactions(bsp);
      }
      _block_started = false;
   }

   /// connect to chain controller irreversible_block signal
   void signal_irreversible_block( const chain::block_state_ptr& bsp ) {
      for(auto& callback : _callbacks) {
         try {
            std::get<1>(callback)(bsp);
         } FC_LOG_AND_DROP(("Failed to pass irreversible block to callback"));
      }
   }


   /// connect to chain controller block_start signal
   void signal_block_start( uint32_t block_num ) {
      if (_block_started & !_alternate_interface) {
         push_transactions(chain::block_state_ptr{});
      }
      _block_started = true;
      for(auto& callback : _callbacks) {
         try {
            std::get<2>(callback)(block_num);
         } FC_LOG_AND_DROP(("Failed to pass block start to callback"));
      }
   }

private:
   void push_transactions( const chain::block_state_ptr& bsp ) {
      for(auto& callback : _callbacks) {
         try {
            std::get<0>(callback)(_trxs, bsp);
         } FC_LOG_AND_DROP(("Failed to pass all transactions and block state to callback"));
      }
      _trxs.clear();
   }
   trx_deque _trxs;
   eosio::chain::vector< std::tuple< applied_transaction_bs_func, irreversible_block_func, block_start_func, accepted_block_func, applied_transaction_func > > _callbacks;
   bool _block_started = false;
   const bool _alternate_interface = false;
};

} // eosio::chain