{-# LANGUAGE PatternSignatures #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Divine.Generator (
    Process, PComp, initial, successors,
    Ptr,
    CString,
    CSize,
    CInt,
    Setup,
    ffi_initialState,
    ffi_getSuccessor,
    ffi_getManySuccessors,
    ffi_getStateSize
    ) where

import qualified Divine.Circular as C
import qualified Divine.Pool as P
import Divine.Blob
import Foreign.Ptr
import Foreign.C.Types
import Foreign.C.String
import Foreign.Marshal.Utils
import Foreign.Storable
import Data.Storable
import Data.Array
import Control.Monad( unless )

class Process p where
    successors :: p -> [p]
    initial :: p

data PComp p q = PComp p q deriving Show

instance (Process p, Process q) => Process (PComp p q) where
    successors (PComp a b) = [ PComp x b | x <- successors a ]
                             ++ [ PComp a x | x <- successors b ]
    {-# INLINE successors #-}
    initial = PComp initial initial

instance forall p q. (Storable p, Storable q) => Storable (PComp p q) where
    peek ptr = do a <- peek (castPtr ptr)
                  b <- peekByteOff (castPtr ptr) (sizeOf (undefined :: p))
                  return $ PComp a b
    poke ptr (PComp a b) = do poke (castPtr ptr) a
                              pokeByteOff (castPtr ptr) (sizeOf a) b
    sizeOf _ = sizeOf (undefined :: p) + sizeOf (undefined :: q)
    alignment _ = alignment (undefined :: p) `lcm` alignment (undefined :: q)

--
-- FFI interface
--

data Setup = Setup P.Pool Int

instance Storable Setup where
  sizeOf _ = 2 * (sizeOf (undefined :: CInt)) + 2 * (sizeOf (undefined :: Ptr ()))
  alignment _ = 4
  peek ptr = do p' <- peekByteOff ptr 0
                p <- peek p'
                s <- peekByteOff ptr (2 * sizeOf (undefined :: Ptr ()))
                return $ Setup p s

ffi_getStateSize :: forall p. (StorableM p) => p -> IO CSize
ffi_getStateSize s = return $ fromIntegral $ sizeOfV s

newBlob :: forall x. (StorableM x) => Ptr Setup -> x -> IO Blob
newBlob setup v = do Setup pool slack <- peek setup
                     blob <- poolBlob pool slack v
                     pokeBlob blob slack v
                     return blob
{-# INLINE newBlob #-}

fromBlob :: forall x. (StorableM x) => Ptr Setup -> Blob -> IO x
fromBlob setup blob = do Setup _ slack <- peek setup
                         peekBlob blob slack

ffi_initialState :: forall p. (Process p, StorableM p) => p -> Ptr Setup -> Ptr Blob -> IO ()
ffi_initialState _ setup to = do
  to' <- newBlob setup (initial :: p)
  poke to to'
{-# INLINE ffi_initialState #-}

ffi_getSuccessor :: forall p. (Show p, Process p, StorableM p) =>
                    p -> Ptr Setup -> CInt -> Blob -> Ptr Blob -> IO CInt
ffi_getSuccessor _ setup handle from to = do
  from' :: p <- fromBlob setup from
  let (res, i) = (numerate' $ successors) from' (fromIntegral handle)
  unless (i == 0) $ do to' <- newBlob setup res
                       poke to to'
  to' <- peek to
  return $ fromIntegral i
{-# INLINE ffi_getSuccessor #-}

ffi_getManySuccessors :: forall p. (Process p, StorableM p) => p -> Int -> Ptr () ->
                         Ptr P.Group -> Ptr (C.Circular Blob) ->
                         Ptr (C.Circular Blob) -> IO ()
ffi_getManySuccessors _ slack p g from to = do
  pool <- P.get p g
  fromQ :: C.Circular Blob <- peek from
  toQ :: C.Circular Blob <- peek to
  let start = C.first fromQ
      count = C.count fromQ
      size = C.size fromQ
      end = start + count
      gen i = do
        from' :: Blob <- C.peekNth fromQ (fromIntegral $ i `mod` size)
        fromSt :: p <- peekBlob from' slack
        let succs = successors fromSt
            inner :: [p] -> IO ()
            inner [] = return ()
            inner (x:xs) = do b <- poolBlob pool slack x
                              C.add from' to
                              C.add b to
                              inner xs
        inner succs
      loop i = if i < end && C.space toQ >= 2 -- FIXME
                   then do gen i
                           loop $ i + 1
                   else return i
  i <- loop start
  C.drop from (i - start)
{-# INLINE ffi_getManySuccessors #-}

instance Storable () where
    sizeOf _ = 0
    alignment _ = 1
    peek p = return ()
    poke p _ = return ()

numerate :: (x -> [x]) -> x -> Int -> (x, Int)
numerate f x i = (arr ! i, if i == max then 0 else i + 1)
    where arr = listArray (1, max) succs
          max = 1 + length succs
          succs = f x

numerate' :: (x -> [x]) -> x -> Int -> (x, Int)
numerate' f x i = (succs !! (i - 1), if i == max then 0 else i + 1)
    where max = 1 + length succs
          succs = f x

