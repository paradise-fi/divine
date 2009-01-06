-- | Derivation for Foreign.Storable's Storable class for serializing values.
--
-- Based on Data.Derive.Binary
--
-- The generated instances implement a very simple tagged data format.
-- First, the (0-based) constructor number is stored, in the smallest
-- of 0, 1, 2, or 4 bytes that can represent the entire range.  Then,
-- the constructor's arguments are stored, in order, using the Storable
-- instances in scope.
--
-- The instanes respect alignment and fill the unused bytes with zeros
-- so that memcmp or hashing may be used for equality tests.
--
-- Do not ever try to generate Storable instances for recursive types.
-- It can't work.
--
-- Written and maintained by Tomas Janousek <tomi@nomi.cz>
--
module Data.Derive.Storable
    ( makeStorable

    -- Export our helper functions.
    , lcm'
    , computeSizeOf
    , align
    , zeroPad

    -- Export these types so that users don't have to import them.
    , Int8
    , Int16
    , Int32
    ) where

import Language.Haskell.TH.All
import Data.List
import Data.Int
import Data.Maybe
import Foreign.Ptr
import Foreign.Storable
import Foreign.Marshal.Array
import Control.Monad

makeStorable :: Derivation
makeStorable = derivation derive "Storable"

derive dat =
        simple_instance "Storable" dat
            [ funN "sizeOf" [defclause 1 $ maximum' $ map ctorSizeOf ctors]
            , funN "alignment" [defclause 1 $ lcm' $ map alignment allTypes]
            , funN "peek" [sclause [vr "p"] pebody]
            , funN "poke" pobody
            ]
    where
        -- Basic definitions.
        ctors = dataCtors dat
        nctors = length ctors
        items :: [(Integer,CtorDef)]
        items = zip [0..] ctors
        allTypes = nub $ concatMap ctorTypes ctors ++ maybeToList tagtype
        myType = lK (qualifiedDataName dat) (map vr typeVars)

        -- | Tag type, if any.
        tagtype | nctors <= 1     = Nothing
                | nctors <= 256   = Just $ lK "Data.Derive.Storable.Int8" []
                | nctors <= 65536 = Just $ lK "Data.Derive.Storable.Int16" []
                | otherwise       = Just $ lK "Data.Derive.Storable.Int32" []

        -- | peek.
        pebody = case tagtype of
            Nothing -> peek_case (head ctors)
            Just t -> bindOffsets 0 (offsets [t]) $
                gtag >>=: ("tag_" ->: case' (vr "tag_") peekCases)
                where gtag = SigE (peekByteOff (vr "p") (vrn 'o' 0)) (lK "IO" [t])
                      peekCases = [ (lit nm, peek_case ctor) | (nm,ctor) <- items ]

        -- | Peek a constructor with its variables.
        peek_case ctor = bindOffsets n ofs peekAll
            where types = ctorTypes ctor
                  construct = return' $ app (ctc ctor) (ctv ctor 'x')
                  (n,ofs) = case tagtype of
                                 Nothing -> (0, offsets types)
                                 Just t  -> (1, drop 2 $ offsets $ t : types)
                  peekBind (n,x) c =
                      peekByteOff (vr "p") (vrn 'o' n) >>=: (x `lam` c)
                  peekAll = foldr peekBind construct $ zip [n..] $ ctv ctor 'x'

        -- | poke.
        pobody = [ sclause [vr "p", ctp ctor 'x'] (poke_case nm ctor)
                   | (nm,ctor) <- items ]

        -- | Poke a constructor with its variables.
        poke_case nm ctor = poke_list
            (te ++ ctv ctor 'x') (tt ++ ctorTypes ctor)
            where (te, tt) = case tagtype of
                                  Nothing -> ([], [])
                                  Just t  -> ([SigE (lit nm) t], [t])

        -- | Poke an unpacked structure.
        poke_list exps types =
            bindOffsets 0 ofs $ sequence__ $ pokeAll ++ [lastPad]
            where ofs = offsets types
                  lastEndVar = if null types
                                  then lit (0 :: Integer)
                                  else vrn 'e' (length types - 1)
                  typeSize = sizeOf myType
                  lastPad = zeroPad (vr "p") lastEndVar typeSize
                  poke1 n x = [ pokeByteOff (vr "p") (vrn 'o' n) x
                              , zeroPad (vr "p") (vrn 'e' n) (vrn 'o' (n + 1))
                              ]
                  pokeAll = safeInit $ concat [ poke1 n x | (n,x) <- zip [0..] exps ]

        -- | Given a list of types, computes the field offsets and ends in an
        -- unpacked structure.
        offsets [] = []
        offsets (t:s) = o : e : offsets' 0 s
            where o = lit (0 :: Integer)
                  e = sizeOf t
        offsets' _ [] = []
        offsets' n (t:s) = o : e : offsets' (n + 1) s
            where o = vrn 'e' n `align` alignment t
                  e = vrn 'o' (n + 1) +: sizeOf t

        -- | Bind offsets to oN, end offsets to eN variables.
        bindOffsets n o = bindMany' $ zip ofsVars o
            where ofsVars = concat [ [vrn 'o' i, vrn 'e' i] | i <- [n..] ]

        -- Variable binding stuff.
        nm `lam` bdy = LamE [nm] bdy
        bind' var val c = app (var `lam` c) [val]
        bindMany' l c = foldr (uncurry bind') c l

        -- Helpers for our exports.
        lcm' = l1 "Data.Derive.Storable.lcm'" . lst
        computeSizeOf = l2 "Data.Derive.Storable.computeSizeOf" zero . lst . map tup
        ctorSizeOf ctor = computeSizeOf
            [ [sizeOf t, alignment t] | t <- maybeToList tagtype ++ ctorTypes ctor]
        align = l2 "Data.Derive.Storable.align"
        zeroPad a b c = lK "Data.Derive.Storable.zeroPad" [a,b,c]

        -- Storable helpers.
        alignment = l1 "alignment" . undefinedOf
        sizeOf = l1 "sizeOf" . undefinedOf
        pokeByteOff a b c = lK "pokeByteOff" [a,b,c]
        peekByteOff = l2 "peekByteOff"

        -- Other helpers.
        maximum' = l1 "maximum" . lst
        zero = lit (0 :: Integer)
        (+:) = l2 "+"
        undefinedOf = SigE (vr "undefined")

        -- | Safe version of Prelude.init.
        safeInit s = if null s then s else init s

        -- Stuff dealing with data definitions with non-zero arity...
        -- (This basically redefines the ctorTypes function so that it returns
        -- t1, t2 ... for type parameters bound in the instance declaration.)
        typeVars = map mkName [ 't' : show n | n <- [1 .. dataArity dat] ]

        dataTypeVars :: DataDef -> [Name]
        dataTypeVars (DataD    _ _ xs _ _) = xs
        dataTypeVars (NewtypeD _ _ xs _ _) = xs

        typeVarSubsts v = case lookup v substitutions of
                               Nothing -> v
                               Just x -> x
            where substitutions = zip (dataTypeVars dat) typeVars

        ctorTypes :: CtorDef -> [Type]
        ctorTypes = map (substType . snd) . ctorStrictTypes

        substType :: Type -> Type
        substType (ForallT ns c t) = ForallT ns c (substType t)
        substType (VarT n) = VarT (typeVarSubsts n)
        substType (AppT t1 t2) = AppT (substType t1) (substType t2)
        substType t = t

-- | Least Common Multiply of a list of integers
lcm' :: Integral a => [a] -> a
lcm' = foldl' lcm 1
{-# INLINE lcm' #-}

-- | Compute the size of a structure containing fields of a given size and
-- alignment contraint.
computeSizeOf :: Int -> [(Int, Int)] -> Int
computeSizeOf sz [] = sz
computeSizeOf sz [(s, _)] = sz + s
computeSizeOf sz ((s, _):l@((_, a):_)) = computeSizeOf (align (sz + s) a) l
{-# INLINE computeSizeOf #-}

-- | Align a given offset using a given alignment constraint.
align :: Int -> Int -> Int
align s a = s + case s `rem` a of
                     0 -> 0
                     x -> a - x
{-# INLINE align #-}

-- | Pad a given offset range with zeros.
zeroPad :: Ptr a -> Int -> Int -> IO ()
zeroPad p o1 o2 = when (o2 > o1) $
    pokeArray (castPtr p `plusPtr` o1) $ replicate (o2 - o1) (0 :: Int8)
{-# INLINE zeroPad #-}
