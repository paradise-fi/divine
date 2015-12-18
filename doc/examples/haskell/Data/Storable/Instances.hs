{-# LANGUAGE PatternSignatures #-}

module Data.Storable.Instances () where

import Data.Storable

instance (StorableM a, StorableM b) => StorableM (a, b) where
    sizeOfM (a, b)     = do sizeOfM a
                            sizeOfM b
    -- For single constructor datatypes, we can use irrefutable patterns to
    -- avoid the need for scoped type variables.
    alignmentM ~(a, b) = do alignmentM a
                            alignmentM b
    peekM              = do a <- peekM
                            b <- peekM
                            return (a, b)
    pokeM (a, b)       = do pokeM a
                            pokeM b

-- For some real life applications, it may be sufficient to use Int16 for the
-- list length.  If that is the case, don't use Data.Storable.Instances.
instance (StorableM a) => StorableM [a] where
    sizeOfM l         = do sizeOfM (0 :: Int)
                           mapM_ sizeOfM l
    alignmentM ~(a:_) = do alignmentM (undefined :: Int)
                           alignmentM a
    peekM             = do n :: Int <- peekM
                           mapM (const peekM) [1..n]
    pokeM l           = do pokeM (length l :: Int)
                           mapM_ pokeM l
