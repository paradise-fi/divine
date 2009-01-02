{-# LANGUAGE ScopedTypeVariables #-}

module Divine.Circular ( Circular, space, count, first,
                         nth, last, add, drop, peekNth, size ) where
import Foreign.Storable
import Foreign.Ptr
import Foreign.C.Types

import Prelude hiding ( last, drop )
data Circular a = Circular { size :: !CInt, count :: !CInt,
                             first :: !CInt, _items :: !(Ptr a) } deriving Show

space c = size c - count c
{-# INLINE space #-}

nth :: forall a. (Storable a) => Circular a -> Int -> Ptr a
nth c i = plusPtr (_items c) (((f + i) `mod` sz) * one)
    where one = sizeOf (undefined :: a)
          sz = fromIntegral $ size c
          f = fromIntegral $ first c
{-# INLINE nth #-}

last :: forall a. (Storable a) => Circular a -> Ptr a
last c = nth c $ fromIntegral $ count c - 1
{-# INLINE last #-}

chCount :: (CInt -> CInt) -> Circular a -> Circular a
chCount f x = x { count = f $ count x }

add :: (Storable a) => a -> Ptr (Circular a) -> IO ()
add a c = do x <- chCount (+1) `fmap` peek c
             poke c x
             poke (last x) a
{-# INLINE add #-}

drop :: (Storable a) => Ptr (Circular a) -> CInt -> IO ()
drop c n = do x <- peek c
              poke c $ x { count = count x - n,
                           first = (first x + n) `mod` size x }
{-# INLINE drop #-}

instance (Storable a) => (Storable (Circular a)) where
    peek p = do s <- peekElemOff (castPtr p) 0
                c <- peekElemOff (castPtr p) 1
                f <- peekElemOff (castPtr p) 2
                let i = plusPtr p $ 3 * sizeOf (undefined :: CInt)
                return Circular { size = s, count = c, first = f, _items = i }
    {-# INLINE peek #-}
    poke p c = do -- pokeElemOff (castPtr p) 0 (size c)
                  pokeElemOff (castPtr p) 1 (count c)
                  pokeElemOff (castPtr p) 2 (first c)
                  return ()
    {-# INLINE poke #-}
    sizeOf _ = 3 * sizeOf (undefined :: CInt) + sizeOf (undefined :: Ptr ())

item :: forall a. (Storable a) => Circular a -> Int -> Ptr a
item c i = plusPtr (_items c) (i * sizeOf (undefined :: a))
{-# INLINE item #-}

peekNth :: (Storable a) => Circular a -> Int -> IO a
peekNth c i = peekElemOff (_items c) i
{-# INLINE peekNth #-}

