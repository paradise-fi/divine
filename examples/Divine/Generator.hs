{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE PatternSignatures #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Divine.Generator (
    System(..),
    mkSystem,

    Ptr,
    CString,
    CSize,
    CInt,
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
import Data.Array
import Control.Monad( unless )

-- | The system datatype.
data System state trans =
    System
    { initialState :: state
    , getSuccessor :: state -> [state]
    }

-- | User-level System constructor
mkSystem ::
    state ->
    (state -> [state]) ->
    System state trans
mkSystem initialState getSuccessor =
    System
    { initialState = initialState
    , getSuccessor = getSuccessor
    }

--
-- FFI interface
--

ffi_getStateSize :: forall state trans. (Storable state) => System state trans -> IO CSize
ffi_getStateSize _ = return $ fromIntegral $ sizeOf (undefined :: state)

ffi_initialState :: (Show state, Storable state) => System state trans -> CString -> IO ()
ffi_initialState system out = do
  let ini = initialState system
      out' = (castPtr :: CString -> Ptr state) out
  poke out' ini
  return ()
{-# INLINE ffi_initialState #-}

ffi_getSuccessor :: forall state trans. (Show state, Storable state)
                    => System state trans ->
                        CInt -> Ptr state -> Ptr state -> IO CInt
ffi_getSuccessor system handle from to = do
  from' <- peek from
  let (res, i) = (numerate' $ getSuccessor system) from' (fromIntegral handle)
  unless (i == 0) $ poke to res
  return $ fromIntegral i
{-# INLINE ffi_getSuccessor #-}

ffi_getManySuccessors :: forall state trans. (Storable state) =>
                         (System state trans) ->
                         Ptr P.Pool -> Ptr P.Group ->
                         Ptr (C.Circular Blob) -> Ptr (C.Circular Blob) -> IO ()
ffi_getManySuccessors system p g from to = do
  pool <- P.get p g
  gen pool
  where gen' pool q = do
          from' :: Blob <- C.peekNth q (fromIntegral $ C.first q)
          fromSt <- peekBlob from'
          C.drop from
          sequence [ do b <- poolBlob pool x
                        C.add from' to
                        C.add b to
                     | x <- getSuccessor system fromSt ]
          gen pool
        gen pool = do
          fromQ :: C.Circular Blob <- peek from
          toQ :: C.Circular Blob <- peek to
          if C.count fromQ > 0 && C.space toQ >= 2
            then gen' pool fromQ
            else return ()

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

