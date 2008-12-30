{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE PatternSignatures #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module BenchmarkHs where

import Data.Binary
import Data.Int
import Foreign.Storable
import Foreign.Ptr
import Divine.Generator

data MyState = MyState !Int16 !Int16
    deriving (Show) {-!derive : Binary !-}

-- TODO Storable (drift) deriving
instance Storable MyState where
    sizeOf _ = 2 * sizeOf (undefined :: Int16)
    alignment _ = alignment (undefined :: Int16)
    peek p = do
	a <- peekElemOff (castPtr p) 0
	b <- peekElemOff (castPtr p) 1
	return (MyState a b)
    poke p (MyState a b) = do
	pokeElemOff (castPtr p) 0 a
	pokeElemOff (castPtr p) 1 b

mysystem :: System MyState ()
mysystem =
    let initialState = MyState 0 0
        succ (MyState a b) | a < 1024 && b < 1024 =
                               [MyState (a + 1) b, MyState a (b + 1)]
                           | otherwise = []
        getSuccessor :: MyState -> Int -> (MyState, Int)
        getSuccessor = numerate' succ
	{- getSuccessor (MyState a b) i | a < 1024 && b < 1024
            = case i of
                1 -> (MyState (a + 1) b, 2)
                2 -> (MyState a (b + 1), 3)
                3 -> (undefined, 0)
                x -> error $ "bad successor handle " ++ show x
            | otherwise = (undefined, 0) -}
    in
	mkSystem initialState getSuccessor

get_state_size = ffi_getStateSize mysystem
get_initial_state t = ffi_initialState mysystem (castPtr t)
get_successor h f t = ffi_getSuccessor mysystem h (castPtr f) (castPtr t)
get_many_successors f t = ffi_getManySuccessors mysystem (castPtr f) (castPtr t)

foreign export ccall
    get_state_size :: IO CSize
foreign export ccall
    get_initial_state :: CString -> IO ()
foreign export ccall
    get_successor :: CInt -> CString -> CString -> IO CInt
