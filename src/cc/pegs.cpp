/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "CCPegs.h"
#include "CCtokens.h"
#include "../importcoin.h"
#include "key_io.h"
#include <gmp.h>


/*
pegs CC is able to create a coin backed (by any supported coin with gateways CC deposits) and pegged to any synthetic price that is able to be calculated based on prices CC
 
 First, the prices CC needs to be understood, so the extensive comments at the top of ~/src/cc/prices.cpp needs to be understood.
 
 The second aspect is the ability to import coins, as used by the crosschain burn/import and the -ac_import chains.
 
 <one hour later...>
 
 OK, now we are ready to describe the pegs CC. Let us imagine an -ac_import sidechain with KMD gateways CC. Now we have each native coin fungible with the real KMD via the gateways deposit/withdraw mechanism. Let us start with that and make a pegged and backed USD chain.
 
 <alternatively we can start from a CC enabled chain with external value>
 
 Here the native coin is KMD, but we want USD, so there needs to be a way to convert the KMD amounts into USD amounts. Something like "KMDBTC, BTCUSD, *, 1" which is the prices CC syntax to calculate KMD/USD, which is exactly what we need. So now we can assume that we have a block by block usable KMD/USD price. implementationwise, there can be an -ac option like -ac_peg="KMDBTC, BTCUSD, *, 1" and in conjunction with -ac_import=KMD gateways CC sidechain, we now have a chain where deposit of KMD issues the correct USD coins and redeem of USD coins releases the correct number of KMD coins.
 
 Are we done yet?
 
 Not quite, as the prices of KMD will be quite volatile relative to USD, which is good during bull markets, not so happy during bear markets. There are 2 halves to this problem, how to deal with massive price increase (easy to solve), how to solve 90% price drop (a lot harder).
 
 In order to solve both, what is needed is an "account" based tracking which updates based on both price change, coins issued, payments made. So let us create an account that is based on a specific pubkey, where all relevant deposits, issuances, withdraws are tracked via appropriate vin/vout markers.
 
 Let us modify the USD chain above so that only 80% of the possible USD is issued and 20% held in reserve. This 80% should be at least some easily changeable #define, or even an -ac parameter. We want the issued coins to be released without any encumberances, but the residual 20% value is still controlled (owned) but the depositor. This account has the amount of KMD deposited and USD issued. At the moment of deposit, there will still be 20% equity left. Let us start with 1000 KMD deposit, $1.5 per KMD -> 800 KMD converted to 1200 USD into depositor pubkey and the account of (1000 KMD, -1200 USD) = 200 KMD or $300 equity.
 
 Now it becomes easy for the bull market case, which is to allow (for a fee like 1%) issuance of more USD as the equity increases, so let us imagine KMD at $10:
 
 (1000 KMD, -1200 USD, 200KMD reserve) -> $2000 equity, issue 80% -> $1600 using 160 KMD
 (1000 KMD, -1200 USD, 200KMD reserve, -160KMD, issue $1600 USD, 40 KMD reserve)
 
 we have $2800 USD in circulation, 40 KMD reserve left against $10000 marketcap of the original deposit. It it easy to see that there are never any problems with lack of KMD to redeem the issued USD in a world where prices only go up. Total USD issuance can be limited by using a decentralized account tracking based on each deposit.
 
 What is evident though is that with the constantly changing price and the various times that all the various deposits issue USD, the global reserves are something that will be hard to predict and in fact needs to be specifically tracked. Let us combine all accounts exposure in to a global reserves factor. This factor will control various max/min/ allowed and fee percentages.
 
 Now we are prepared to handle the price goes down scenario. We can rely on the global equity/reserve ratio to be changing relatively slowly as the reference price is the smooted trustless oracles price. This means there will be enough blocks to adjust the global reserves percentage. What we need to do is liquidate specific positions that have the least reserves.
 
 What does liquidation mean? It means a specific account will be purchased at below its current value and the KMD withdrawn. Let us assume the price drops to $5:
 
 (1000 KMD, -1200 USD, 200KMD reserve, -160KMD, issue $1600 USD, 40 KMD reserve) 1000 KMD with 2800 USD issued so $2200 reserves. Let us assume it can be liquidated at a 10% discount, so for $2000 in addition to the $2800, the 5000 KMD is able to be withdrawn. This removes 4800 USD coins for 1000 KMD, which is a very low reserve amount of 4%. If a low reserve amount is removed from the system, then the global reserve amount must be improved.
 
 In addition to the global reserves calculation, there needs to be a trigger percentage that enables positions to be liquidated. We also want to liquidate the worst positions, so in addition to the trigger percentage, there should be a liquidation threshold, and the liquidator would need to find 3 or more better positions that are beyond the liquidation threshold, to be able to liquidate. This will get us to at most 3 accounts that could be liquidated but are not able to, so some method to allow those to also be liquidated. The liquidating nodes are making instant profits, so they should be expected to do whatever blockchain scanning and proving to make things easy for the rest of the nodes.
 
 One last issue is the normal redemption case where we are not liquidating. In this case, it should be done at the current marketprice, should improve the global reserves metrics and not cause anybody whose position was modified to have cause for complaint. Ideally, there would be an account that has the identical to the global reserve percentage and also at the same price as current marketprice, but this is not realistic, so we need to identify classes of accounts and consider which ones can be fully or partially liquidated to satisfy the constraints.
 
 looking at our example account:
 (1000 KMD, -1200 USD, 200KMD reserve, -160KMD, issue $1600 USD, 40 KMD reserve)

 what sort of non-liquidation withdraw would be acceptable? if the base amount 1000 KMD is reduced along with USD owed, then the reserve status will go up for the account. but that would seem to allow extra USD to be able to be issued. there should be no disadvantage from funding a withdraw, but also not any large advantage. it needs to be a neutral event.... 
 
 One solution is to allow for the chance for any account to be liquidated, but the equity compensated for with a premium based on the account reserves. So in the above case, a premium of 5% on the 40KMD reserve is paid to liquidate its account. Instead of 5% premium, a lower 1% can be done if based on the MAX(correlated[daywindow],smoothed) so we get something that is close to the current marketprice. To prevent people taking advantage of the slowness of the smoothed price to adjust, there would need to be a one day delay in the withdraw. 
 
 From a practical sense, it seems a day is a long time, so maybe having a way to pay a premium like 10%, or wait a day to get the MAX(correlated[daywindow],smoothed) price. This price "jumping" might also be taken advantage of in the deposit side, so similar to prices CC it seems good to have the MAX(correlated[daywindow],smoothed) method.
 
 Now, we have a decentralized mechanism to handle the price going lower! Combined with the fully decentralized method new USD coins are issued, makes this argubably the first decentralized blockchain that is both backed and pegged. There is the reliance on the gateways CC multisig signers, so there is a fundamental federated trust for chains without intrinsic value.
 
 Also, notice that the flexibly syntax of prices CC allows to define pegs easily for virtually any type of synthetic, and all the ECB fiats can easily get a backed and pegged coin.
 
 Let us now consider how to enforce a peg onto a specific gateways CC token. If this can also be achieved, then a full DEX for all the different gateways CC supported coins can be created onto a single fiat denominated chain.
 
 I think just having a pegscreate rpc call that binds an existing gateways create to a price CC syntax price will be almost enough to support this. Let us assume a USD stablechain and we have a BTC token, then pegscreate <btc gateways txid> "BTCUSD, 1"
 that will specify using the BTCUSD price, so now we need to create a <txid> based way to do tokenbid/tokenask. For a <txid> based price, the smoothed price is substituted.
 
 There is the issue of the one day delay, so it might make sense to allow specific bid/ask to be based on some simple combinations of the three possible prices. it might even be possible to go a bit overboard and make a forth like syntax to define the dynamic price for a bid, which maybe at times wont be valid, like it is only valid if the three prices are within 1% of each other. But all that seems over complex and for initial release it can just use the mined, correlated or smoothed price, with some specified percentage offset
 
 Implementation notes:
    make sure that fees and markers that can be sent to an unspendable address are sent to: RNdqHx26GWy9bk8MtmH1UiXjQcXE4RKK2P, this is the address for BOTS

 
 */

// start of consensus code
#ifndef PEGS_THRESHOLDS
#define PEGS_THRESHOLDS
#define PEGS_ACCOUNT_MAX_DEBT 80
#define PEGS_GLOBAL_RED_ZONE 60
#define PEGS_ACCOUNT_YELLOW_ZONE 60
#define PEGS_ACCOUNT_RED_ZONE 90
#endif // PEGS_THRESHOLDS
#define CC_MARKER_VALUE 1000
#define CC_TXFEE 10000

extern uint64_t ASSETCHAINS_PEGSCCPARAMS[3];

extern uint8_t DecodeGatewaysBindOpRet(char *depositaddr,const CScript &scriptPubKey,uint256 &tokenid,std::string &coin,int64_t &totalsupply,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &gatewaypubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2,uint8_t &wiftype);
// see include extern int64_t GetTokenBalance(CPubKey pk, uint256 tokenid);
extern int32_t komodo_currentheight();
extern int32_t prices_syntheticvec(std::vector<uint16_t> &vec, std::vector<std::string> synthetic);
extern int64_t prices_syntheticprice(std::vector<uint16_t> vec, int32_t height, int32_t minmax, int16_t leverage);

CScript EncodePegsCreateOpRet(std::vector<uint256> bindtxids)
{
    CScript opret; uint8_t evalcode = EVAL_PEGS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'C' << bindtxids);        
    return(opret);
}

uint8_t DecodePegsCreateOpRet(const CScript &scriptPubKey,std::vector<uint256> &bindtxids)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_PEGS && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bindtxids) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodePegsAccountOpRet(uint8_t funcid,uint256 tokenid,uint256 pegstxid,CPubKey srcpub,int64_t amount,std::pair <int64_t,int64_t> account,CPubKey accountpk)
{
    CScript opret; uint8_t evalcode=EVAL_PEGS; struct CCcontract_info *cp,C; CPubKey pegspk;
    std::vector<CPubKey> pubkeys; vscript_t vopret;

    cp = CCinit(&C,EVAL_PEGS);
    pegspk = GetUnspendable(cp,0);
    pubkeys.push_back(accountpk);
    if (srcpub!=accountpk) pubkeys.push_back(srcpub);
    vopret = E_MARSHAL(ss << evalcode << funcid << pegstxid << srcpub << amount << account << accountpk);        
    return(EncodeTokenOpRetV1(tokenid,pubkeys, { vopret }));
}

uint8_t DecodePegsAccountOpRet(const CScript &scriptPubKey,uint256 &tokenid,uint256 &pegstxid,CPubKey &srcpub,int64_t &amount,std::pair <int64_t,int64_t> &account,CPubKey& accountpk)
{
    std::vector<vscript_t>  oprets;
    std::vector<uint8_t> vopret,vOpretExtra; uint8_t *script,e,f; std::vector<CPubKey> pubkeys;

    if (DecodeTokenOpRetV1(scriptPubKey,tokenid,pubkeys, oprets)!=0 && GetOpReturnCCBlob(oprets, vOpretExtra) && vOpretExtra.size()>0)
    {
        vopret=vOpretExtra;
    }
    else GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_PEGS && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> pegstxid; ss >> srcpub; ss >> amount; ss >> account; ss >> accountpk) != 0 )
    {
        return(f);
    }
    return(0);
}

uint8_t DecodePegsGetOpRet(const CTransaction tx,uint256& pegstxid,uint256 &tokenid,CPubKey &srcpub,int64_t &amount,std::pair<int64_t,int64_t> &account,CPubKey &accountpk)
{
    std::vector<uint8_t> vopret; uint8_t *script; 
    ImportProof proof; CTransaction burntx; std::vector<CTxOut> payouts;

    GetOpReturnData(tx.vout[tx.vout.size()-1].scriptPubKey, vopret);
    
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_IMPORTCOIN && UnmarshalImportTx(tx,proof,burntx,payouts) && UnmarshalBurnTx(burntx,pegstxid,tokenid,srcpub,amount,account,accountpk))
    {
        return('G');
    }
    return(0);
}

uint8_t DecodePegsOpRet(CTransaction tx,uint256& pegstxid,uint256& tokenid)
{
    std::vector<vscript_t>  oprets; int32_t numvouts=tx.vout.size();
    std::vector<uint8_t> vopret,vOpretExtra; uint8_t *script,e,f; std::vector<CPubKey> pubkeys;
    ImportProof proof; CTransaction burntx; std::vector<CTxOut> payouts; uint256 tmppegstxid; CPubKey srcpub,accountpk; int64_t amount; std::pair<int64_t,int64_t> account;

    if (numvouts<1) return 0;
    if (DecodeTokenOpRetV1(tx.vout[numvouts-1].scriptPubKey,tokenid,pubkeys, oprets)!=0 && GetOpReturnCCBlob(oprets, vOpretExtra) && vOpretExtra.size()>0)
    {
        vopret=vOpretExtra;
    }
    else GetOpReturnData(tx.vout[numvouts-1].scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if (tx.IsPegsImport())
        return(DecodePegsGetOpRet(tx,pegstxid,tokenid,srcpub,amount,account,accountpk));
    else if ( vopret.size() > 2 && script[0] == EVAL_PEGS)
    {
        E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> pegstxid);
        if (f == 'C' || f == 'F' || f == 'R' || f == 'X' || f == 'E' || f == 'L')
            return(f);
    }
    return(0);
}

int64_t IsPegsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool PegsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        //fprintf(stderr,"vini.%d\n",i);
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            //fprintf(stderr,"vini.%d check mempool\n",i);
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                //fprintf(stderr,"vini.%d check hash and vout\n",i);
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant Pegs from mempool");
                if ( (assetoshis= IsPegsvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsPegsvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

std::string PegsDecodeAccountTx(CTransaction tx,CPubKey& pk,int64_t &amount,std::pair<int64_t,int64_t> &account,CPubKey &accountpk)
{
    uint256 hashBlock,tokenid,pegstxid; int32_t numvouts=tx.vout.size(); char funcid;

    if ((funcid=DecodePegsOpRet(tx,pegstxid,tokenid))!=0)
    {
        switch(funcid)
        {            
            case 'F': if (DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,pk,amount,account,accountpk)=='F') return("fund");
                      break;
            case 'G': if (DecodePegsGetOpRet(tx,pegstxid,tokenid,pk,amount,account,accountpk)=='G') return("get");
                      break;
            case 'R': if (DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,pk,amount,account,accountpk)=='R') return("redeem");
                      break;
            case 'X': if (DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,pk,amount,account,accountpk)=='X') return("close");
                      break;
            case 'E': if (DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,pk,amount,account,accountpk)=='E') return("exchange");
                      break;
            case 'L': if (DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,pk,amount,account,accountpk)=='L') return("liquidate");
                      break;
        }
    }
    return ("");
}

char PegsFindAccount(struct CCcontract_info *cp,CPubKey pk,uint256 pegstxid, uint256 tokenid, uint256 &accounttxid, std::pair<int64_t,int64_t> &account)
{
    char coinaddr[64]; int64_t nValue,tmpamount; uint256 txid,spenttxid,hashBlock,tmptokenid,tmppegstxid;
    CTransaction tx,acctx; int32_t numvouts,vout,ratio; char funcid,f; CPubKey pegspk,tmppk,accountpk;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    ImportProof proof; CTransaction burntx; std::vector<CTxOut> payouts;

    accounttxid=zeroid;
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,pk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "txid=" << txid.GetHex() << ", vout=" << vout << ", nValue=" << nValue << std::endl);
        if (vout == 1 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (f=DecodePegsOpRet(tx,tmppegstxid,tmptokenid))!=0 && pegstxid==tmppegstxid && tokenid==tmptokenid)
        {            
            accounttxid=txid;
            funcid=f;
            acctx=tx;
        }
    }
    if (accounttxid!=zeroid)
    {
        PegsDecodeAccountTx(acctx,tmppk,tmpamount,account,accountpk);
        return(funcid);
    }
    else return(0);
}

int64_t PegsGetTokenPrice(uint256 tokenid)
{
    int64_t price; CTransaction tokentx; uint256 hashBlock; std::vector<uint16_t> exp;
    std::string name,desc; std::vector<uint8_t> vorigpubkey; int32_t numvouts;

    if (myGetTransaction(tokenid,tokentx,hashBlock)!=0 && (numvouts=tokentx.vout.size())>0 && DecodeTokenCreateOpRetV1(tokentx.vout[numvouts-1].scriptPubKey,vorigpubkey,name,desc)=='c')
    {
        std::vector<std::string> vexpr;
        SplitStr(desc, vexpr);
        if (prices_syntheticvec(exp, vexpr)>=0 && (price = prices_syntheticprice(exp, komodo_currentheight(), 0, 1))>=0)
            return (price);
    }
    return (0);
}

std::string PegsGetTokenName(uint256 tokenid)
{
    CTransaction tokentx; uint256 hashBlock; std::string name,desc; std::vector<uint8_t> vorigpubkey; int32_t numvouts;

    if (myGetTransaction(tokenid,tokentx,hashBlock)!=0 && (numvouts=tokentx.vout.size())>0 && DecodeTokenCreateOpRetV1(tokentx.vout[numvouts-1].scriptPubKey,vorigpubkey,name,desc)=='c')
    {
        return (name);
    }
    LOGSTREAM("pegscc",CCLOG_ERROR, stream << "cant find token create or invalid tokenid " << tokenid.GetHex() << std::endl);
    return("");
}

int64_t PegsGetTokensAmountPerPrice(int64_t amount,uint256 tokenid)
{      
    mpz_t res,a,b;
    int64_t price=PegsGetTokenPrice(tokenid);
    
    if (price==0) return (0);
    mpz_init(res);
    mpz_init(a);
    mpz_init(b);
    mpz_set_si(a, amount);
    mpz_set_si(b, COIN);
    mpz_mul(res, a, b);
    mpz_set_si(a, price);   
    mpz_tdiv_q(res, res, a);
    return (mpz_get_si(res));           
}

double PegsGetRatio(uint256 tokenid,std::pair<int64_t,int64_t> account)
{      
    mpz_t res,a,b;
    mpz_init(res);
    mpz_init(a);
    mpz_init(b);
    mpz_set_si(a, account.first);
    mpz_set_si(b, PegsGetTokenPrice(tokenid));
    mpz_mul(res, a, b);
    mpz_set_si(a, COIN);
    mpz_tdiv_q(res, res, a);
    return ((double)account.second)*100/mpz_get_si(res);           
}

double PegsGetAccountRatio(uint256 pegstxid,uint256 tokenid,uint256 accounttxid)
{
    int64_t amount; uint256 hashBlock,tmptokenid,tmppegstxid;
    CTransaction tx; int32_t numvouts; char funcid; CPubKey pk,accountpk;
    std::pair<int64_t,int64_t> account; struct CCcontract_info *cp,C;

    cp = CCinit(&C,EVAL_PEGS);
    if (myGetTransaction(accounttxid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
        (funcid=DecodePegsOpRet(tx,tmppegstxid,tmptokenid))!=0 && pegstxid==tmppegstxid && tokenid==tmptokenid)
    {  
        PegsDecodeAccountTx(tx,pk,amount,account,accountpk);           
        return PegsGetRatio(tokenid,account);        
    }
    return (0);
}

double PegsGetGlobalRatio(uint256 pegstxid)
{
    char coinaddr[64]; int64_t nValue,amount,globaldebt=0; uint256 txid,accounttxid,hashBlock,tmppegstxid,tokenid;
    CTransaction tx; int32_t numvouts,vout; char funcid; CPubKey mypk,pegspk,pk,accountpk;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; std::pair<int64_t,int64_t> account;
    std::map<uint256,std::pair<int64_t,int64_t>> globalaccounts;
    struct CCcontract_info *cp,C;

    cp = CCinit(&C,EVAL_PEGS);
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,pegspk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if (vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (funcid=DecodePegsOpRet(tx,tmppegstxid,tokenid))!=0 && pegstxid==tmppegstxid && (funcid=='F' || funcid=='G' || funcid=='E'))
        {              
            PegsDecodeAccountTx(tx,pk,amount,account,accountpk);
            globalaccounts[tokenid].first+=account.first;
            globalaccounts[tokenid].second+=account.second;
        }
    }
    unspentOutputs.clear();
    GetTokensCCaddress(cp,coinaddr,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if (myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 && DecodePegsOpRet(tx,tmppegstxid,tokenid)!=0 && pegstxid==tmppegstxid)
        {
            globalaccounts[tokenid].first+=nValue;
        }
    }
    mpz_t res,globaldeposit,a,b;
    mpz_init(res);
    mpz_init(globaldeposit);
    mpz_init(a);
    mpz_init(b);
    mpz_set_si(globaldeposit, 0);
    for (std::map<uint256,std::pair<int64_t,int64_t>>::iterator it = globalaccounts.begin(); it != globalaccounts.end(); ++it)
    {
        mpz_set_si(res, 0);
        mpz_set_si(a, globalaccounts[it->first].first);
        mpz_set_si(b, PegsGetTokenPrice(it->first));
        mpz_mul(res,a,b);
        mpz_add(globaldeposit,globaldeposit,res);
        globaldebt+=globalaccounts[it->first].second;
    }
    if (globaldebt>0)
    {       
        mpz_set_si(res, 0);
        mpz_set_si(a, COIN);
        mpz_tdiv_q(res, globaldeposit, a);
        return ((double)globaldebt)*100/mpz_get_si(res); 
    }
    return (0);
}

std::string PegsFindSuitableAccount(struct CCcontract_info *cp,uint256 pegstxid, uint256 tokenid, int64_t tokenamount,uint256 &accounttxid, std::pair<int64_t,int64_t> &account)
{
    char coinaddr[64]; int64_t nValue,tmpamount; uint256 txid,hashBlock,tmptokenid,tmppegstxid;
    CTransaction tx,acctx; int32_t numvouts,vout; char funcid,f; CPubKey pegspk,tmppk,accountpk;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    ImportProof proof; CTransaction burntx; std::vector<CTxOut> payouts; double ratio,maxratio=0;
    std::pair<int64_t,int64_t> tmpaccount;

    accounttxid=zeroid;
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,pegspk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "txid=" << txid.GetHex() << ", vout=" << vout << ", nValue=" << nValue << std::endl);
        if (vout == 0 && nValue == CC_MARKER_VALUE && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,0) == 0 &&
            (ratio=PegsGetAccountRatio(pegstxid,tokenid,txid))>=(ASSETCHAINS_PEGSCCPARAMS[2]?ASSETCHAINS_PEGSCCPARAMS[2]:PEGS_ACCOUNT_YELLOW_ZONE) && ratio>maxratio)
        {   
            if (myGetTransaction(txid,tx,hashBlock)!=0 && !PegsDecodeAccountTx(tx,tmppk,tmpamount,tmpaccount,accountpk).empty() && tmpaccount.first>=tokenamount)
            {
                accounttxid=txid;
                acctx=tx;
                maxratio=ratio;
            }
        }
    }
    if (!maxratio)
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            vout = (int32_t)it->first.index;
            nValue = (int64_t)it->second.satoshis;
            LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "txid=" << txid.GetHex() << ", vout=" << vout << ", nValue=" << nValue << std::endl);
            if (vout == 0 && nValue == CC_MARKER_VALUE && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,0) == 0 && (ratio=PegsGetAccountRatio(pegstxid,tokenid,txid))>maxratio)
            {   
                if (myGetTransaction(txid,tx,hashBlock)!=0 && !PegsDecodeAccountTx(tx,tmppk,tmpamount,tmpaccount,accountpk).empty() && tmpaccount.first>=tokenamount)
                {
                    accounttxid=txid;
                    acctx=tx;
                    maxratio=ratio;
                }
            }
        }
    if (accounttxid!=zeroid)
    {
        return(PegsDecodeAccountTx(acctx,tmppk,tmpamount,account,accountpk));
    }
    else return("");
}

bool CheckSynthetic(std::string description)
{
    std::vector<std::string> vexpr; std::vector<uint16_t> exp;

    SplitStr(description, vexpr);
    if (prices_syntheticvec(exp, vexpr)<0) return (false);
    return (true);
}

std::string ValidateAccount(const CTransaction &tx, const uint256 &tokenid,const std::pair <int64_t,int64_t> &prevaccount)
{
    struct CCcontract_info *cp,C; CPubKey pegspk,pk,accountpk; char addr[64]; int64_t amount; std::pair <int64_t,int64_t> account(0,0);
    std::string name=PegsDecodeAccountTx(tx,pk,amount,account,accountpk);

    cp = CCinit(&C,EVAL_PEGS);
    pegspk=GetUnspendable(cp,0);
    if ( (*cp->ismyvin)(tx.vin[0].scriptSig) == 0 )
        return ("vin.0 is CC account marker for pegs"+name+"!");
    else if ( (*cp->ismyvin)(tx.vin[1].scriptSig) == 0 )
        return ("vin.1 is CC account marker for pegs"+name+"!");
    else if ( GetCCaddress1of2(cp,addr,pegspk,pegspk) && ConstrainVout(tx.vout[0],1,addr,CC_MARKER_VALUE)==0)
        return ("invalid account marker vout.0 for pegs"+name+"!");
    else if ( GetCCaddress1of2(cp,addr,accountpk,pegspk) && ConstrainVout(tx.vout[1],1,addr,CC_MARKER_VALUE)==0)
        return ("invalid account marker vout.1 for pegs"+name+"!");
    else if (name=="fund" && (prevaccount.first+amount!=account.first || prevaccount.second!=account.second || pk!=accountpk))
            return ("invalid previous and current account comparisons!");
    else if (name=="redeem" && (prevaccount.first-amount!=account.first || prevaccount.second!=account.second || pk!=accountpk))
            return ("invalid previous and current account comparisons!");
    else if (name=="close" && (account.first!=0 || prevaccount.second-amount!=0 || account.second!=0 || pk!=accountpk))
            return ("invalid previous and current account comparisons!");
    else if (name=="exchange" && (prevaccount.first-PegsGetTokensAmountPerPrice(amount,tokenid)!=account.first || prevaccount.second-amount!=account.second))
            return ("invalid previous and current account comparisons!");
    else if (name=="liquidate" && (prevaccount.second-amount!=account.second || account.first!=0 || account.second!=0))
            return ("invalid previous and current account comparisons!");
    return ("");
}

bool PegsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid,pegstxid,tokenid,accounttxid,tmpaccounttxid,hashBlock;
    uint8_t funcid; char str[65],destaddr[64],addr[64]; int64_t amount; std::pair <int64_t,int64_t> account(0,0),prevaccount(0,0),tmpaccount(0,0); 
    CPubKey srcpub,pegspk,accountpk; std::string error,name,description; std::vector<uint256> bindtxids; CTransaction tmptx; std::vector<uint8_t> vorigpubkey;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        // if ( PegsExactAmounts(cp,eval,tx,1,10000) == false )
        // {
        //     fprintf(stderr,"Pegsget invalid amount\n");
        //     return false;
        // }
        // else
        // {
            txid = tx.GetHash();
            CCOpretCheck(eval,tx,true,true,true);
            ExactAmounts(eval,tx,CC_TXFEE);
            if ((funcid=DecodePegsOpRet(tx,pegstxid,tokenid)) !=0 )
            {
                pegspk=GetUnspendable(cp,0);
                if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
                    return eval->Invalid("invalid pegs txid, for this chain only valid pegs txid is"+KOMODO_EARLYTXID.GetHex());                
                switch (funcid)
                {
                    case 'C':
                        //vin.0: normal input
                        //vout.0-99: CC vouts for pegs funds
                        //vout.1: CC vout marker                        
                        //vout.n-1: opreturn - 'B' tokenid coin totalsupply oracletxid M N pubkeys taddr prefix prefix2 wiftype
                        return eval->Invalid("unexpected PegsValidate for pegscreate!");
                        break;
                    case 'F':
                        //if account exists:
                        //vin.0: marker input from account
                        //vin.1: marker input from account
                        //else
                        //vin.0: input from pegsCC global address
                        //vout.0: CC vout account marker (1of2 of mypk and pegspk)
                        //vout.1: CC vout account marker (1of2 of pegspk and pegspk)if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
                        //vout.2: tokens to 1of2 address of mypk and pegspk
                        //vout.3: CC change of tokens if exists
                        //vout.4: CC change back to pegs global address if exists           
                        //vout.n-1: opreturn - 'F' tokenid pegstxid mypk amount account
                        if (myGetTransaction(pegstxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
                            return eval->Invalid("invalid pegs txid!"); 
                        else if (DecodePegsCreateOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
                            return eval->Invalid("invalid pegscreate OP_RETURN data!");
                        else if (myGetTransaction(tokenid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
                            return eval->Invalid("invalid token id!"); 
                        else if (DecodeTokenCreateOpRetV1(tmptx.vout[numvouts-1].scriptPubKey,vorigpubkey,name,description)!='c')
                            return eval->Invalid("invalid token OP_RETURN data!");
                        else if (!CheckSynthetic(description))
                            return eval->Invalid("invalid synthetic in token description field. You must put the price synthetic in token description field!");
                        else if ((numvouts=tx.vout.size()) < 1 || DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,srcpub,amount,account,accountpk)!='F')
                            return eval->Invalid("invalid pegsfund OP_RETURN data!"); 
                        else if (PegsFindAccount(cp,srcpub,pegstxid,tokenid,accounttxid,prevaccount)!=0 && !(error=ValidateAccount(tx,tokenid,prevaccount)).empty())
                            return eval->Invalid(error);
                        else if (accounttxid==zeroid)
                        {
                            if ((*cp->ismyvin)(tx.vin[0].scriptSig) == 0 )
                                return eval->Invalid("vin.0 is CC for pegsfund!");
                            else if ( GetCCaddress1of2(cp,addr,pegspk,pegspk) && ConstrainVout(tx.vout[0],1,addr,CC_MARKER_VALUE)==0)
                                return eval->Invalid("invalid account marker vout.0 for pegsfund!");
                            else if ( GetCCaddress1of2(cp,addr,srcpub,pegspk) && ConstrainVout(tx.vout[1],1,addr,CC_MARKER_VALUE)==0)
                                return eval->Invalid("invalid account marker vout.1 for pegsfund!");
                            else if (amount!=account.first || account.second!=0)
                                return eval->Invalid("different amount and account state!");
                        }
                        else if (GetTokensCCaddress1of2(cp,addr,srcpub,pegspk) && ConstrainVout(tx.vout[2],1,addr,amount)==0)
                            return ("invalid tokens destination or amount vout.2 for pegsfund!");
                        break;
                    case 'R':
                        //vin.0: marker input from account
                        //vin.1: marker input from account
                        //vout.0: CC vout account marker (1of2 of mypk and pegspk)
                        //vout.1: CC vout account marker (1of2 of pegspk and pegspk)          
                        //vout.n-1: opreturn - 'R' tokenid pegstxid mypk amount account
                        if ((numvouts=tx.vout.size()) < 1 || DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,srcpub,amount,account,accountpk)!='R')
                            return eval->Invalid("invalid pegsredeem OP_RETURN data!");
                        else if (PegsFindAccount(cp,srcpub,pegstxid,tokenid,accounttxid,prevaccount)==0)
                            return eval->Invalid("no account found to redeem from, please create account first with pegsfund!");
                        else if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0 && ignoretxid!=tx.GetHash())
                            return eval->Invalid("previous account tx not yet confirmed!");
                        else if (!(error=ValidateAccount(tx,tokenid,prevaccount)).empty())
                            return eval->Invalid(error);
                        else if (PegsGetRatio(tokenid,account)>=PEGS_ACCOUNT_MAX_DEBT)
                            return eval->Invalid(std::string("cannot redeem when account ratio >= ") + std::to_string(PEGS_ACCOUNT_MAX_DEBT) + "%%!");
                        else if (amount>account.first-(PegsGetTokensAmountPerPrice(account.second,tokenid)*100/PEGS_ACCOUNT_MAX_DEBT))
                            return eval->Invalid(std::string("cannot redeem this amount of tokens, you must leave enough tokens to have account ratio <= ") + std::to_string(PEGS_ACCOUNT_MAX_DEBT) + "%%!");
                        else if (_GetCCaddress(addr,EVAL_TOKENS,srcpub) && ConstrainVout(tx.vout[2],1,addr,amount)==0)
                            return ("invalid tokens destination or amount vout for pegsredeem!");
                        else if (numvouts>3 && GetTokensCCaddress1of2(cp,addr,srcpub,pegspk) && ConstrainVout(tx.vout[3],1,addr,prevaccount.first-amount)==0)
                            return ("invalid tokens destination or amount vout for pegsredeem!");
                        break;   
                    case 'X':
                        //vin.0: marker input from account
                        //vin.1: marker input from account
                        //vout.0: CC vout account marker (1of2 of mypk and pegspk)
                        //vout.1: CC vout account marker (1of2 of pegspk and pegspk)          
                        //vout.n-1: opreturn - 'X' tokenid pegstxid mypk amount account
                        if ((numvouts=tx.vout.size()) < 1 || DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,srcpub,amount,account,accountpk)!='X')
                            return eval->Invalid("invalid pegsclose OP_RETURN data!"); 
                        else if (PegsFindAccount(cp,srcpub,pegstxid,tokenid,accounttxid,prevaccount)==0)
                            return eval->Invalid("no account found to close, please create account first with pegsfund!");
                        else if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0 && ignoretxid!=tx.GetHash())
                            return eval->Invalid("previous account tx not yet confirmed!");
                        else if (!(error=ValidateAccount(tx,tokenid,prevaccount)).empty())
                            return eval->Invalid(error);
                        else if (_GetCCaddress(addr,EVAL_TOKENS,srcpub) && ConstrainVout(tx.vout[2],1,addr,prevaccount.first)==0)
                            return ("invalid tokens destination or amount vout.2 for pegsclose!");
                        else if ( Getscriptaddress(addr,CScript() << ParseHex(HexStr(CCtxidaddr(addr,pegstxid))) << OP_CHECKSIG) && ConstrainVout(tx.vout[3],0,addr,prevaccount.second)==0)
                            return ("invalid coins destination or amount vout.3 for pegsclose!");
                        break;     
                    case 'E':
                        //vin.0: marker input from account
                        //vin.1: marker input from account
                        //vout.0: CC vout account marker (1of2 of mypk and pegspk)
                        //vout.1: CC vout account marker (1of2 of pegspk and pegspk)          
                        //vout.n-1: opreturn - 'E' tokenid pegstxid mypk amount account
                        if ((numvouts=tx.vout.size()) < 1 || DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,srcpub,amount,account,accountpk)!='E')
                            return eval->Invalid("invalid pegsexchange OP_RETURN data!"); 
                        else if (PegsFindAccount(cp,accountpk,pegstxid,tokenid,accounttxid,prevaccount)==0)
                            return eval->Invalid("no account found to exchange coins!");
                        else if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0 && ignoretxid!=tx.GetHash())
                            return eval->Invalid("previous account tx not yet confirmed!");
                        else if (!(error=ValidateAccount(tx,tokenid,prevaccount)).empty())
                            return eval->Invalid(error);
                        else if (PegsGetAccountRatio(pegstxid,tokenid,accounttxid)<(ASSETCHAINS_PEGSCCPARAMS[2]?ASSETCHAINS_PEGSCCPARAMS[2]:PEGS_ACCOUNT_YELLOW_ZONE))
                            return eval->Invalid("cannot exchange coins from account that is not yellow zone!");
                        else if ((PegsFindSuitableAccount(cp,pegstxid,tokenid,amount,tmpaccounttxid,tmpaccount)).empty() || tx.vin[0].prevout.hash!=tmpaccounttxid || tx.vin[1].prevout.hash!=tmpaccounttxid)
                            return eval->Invalid("cannot exchange from this account, it is not worst account there is!");
                        else if (_GetCCaddress(addr,EVAL_TOKENS,srcpub) && ConstrainVout(tx.vout[2],1,addr,prevaccount.first-account.first)==0)
                            return ("invalid tokens destination or amount vout.2 for pegsexchange!");
                        else if (Getscriptaddress(addr,CScript() << ParseHex(HexStr(CCtxidaddr(addr,pegstxid))) << OP_CHECKSIG) && ConstrainVout(tx.vout[3],0,addr,amount)==0)
                            return ("invalid coins destination or amount vout.3 for pegsexchange, it should be coin burn vout!");
                        else if (numvouts>4 && GetTokensCCaddress1of2(cp,addr,accountpk,pegspk) && ConstrainVout(tx.vout[4],1,addr,account.first)==0)
                            return ("invalid tokens destination or amount vout.4 for pegsexchange, it should be the change of tokens back to account address!");
                        else if (numvouts>5 && GetCCaddress(cp,addr,pegspk) && ConstrainVout(tx.vout[5],1,addr,0)==0)
                            return ("invalid coins destination or amount vout.5 for pegsexchange, it should be change back to pegs CC global address!");
                        break;
                    case 'L':
                        //vin.0: marker input from account
                        //vin.1: marker input from account
                        //vout.0: CC vout account marker (1of2 of mypk and pegspk)
                        //vout.1: CC vout account marker (1of2 of pegspk and pegspk)          
                        //vout.n-1: opreturn - 'L' tokenid pegstxid mypk amount account
                        if ((numvouts=tx.vout.size()) < 1 || DecodePegsAccountOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,pegstxid,srcpub,amount,account,accountpk)!='L')
                            return eval->Invalid("invalid pegsliquidate OP_RETURN data!"); 
                        else if (PegsFindAccount(cp,accountpk,pegstxid,tokenid,accounttxid,prevaccount)==0)
                            return eval->Invalid("cannot find the account to liquidate!");
                        else if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0 && ignoretxid!=tx.GetHash())
                            return eval->Invalid("previous liquidation account tx not yet confirmed");
                        else if (!(error=ValidateAccount(tx,tokenid,prevaccount)).empty())
                            return eval->Invalid(error);
                        else if (PegsGetRatio(tokenid,prevaccount)<(ASSETCHAINS_PEGSCCPARAMS[0]?ASSETCHAINS_PEGSCCPARAMS[0]:PEGS_ACCOUNT_RED_ZONE))
                            return eval->Invalid("cannot liquidate account that is not in the red zone!");
                        else if (_GetCCaddress(addr,EVAL_TOKENS,srcpub) && ConstrainVout(tx.vout[2],1,addr,amount)==0)
                            return ("invalid tokens destination or amount vout.2 for pegsliquidate!");
                        else if (Getscriptaddress(addr,CScript() << ParseHex(HexStr(CCtxidaddr(addr,pegstxid))) << OP_CHECKSIG) && ConstrainVout(tx.vout[3],0,addr,prevaccount.second)==0)
                            return ("invalid coins destination or amount vout.3 for pegsliquidate, it should be coin burn vout!");
                        else if (GetTokensCCaddress(cp,addr,pegspk) && ConstrainVout(tx.vout[4],1,addr,prevaccount.first-amount)==0)
                            return ("invalid tokens destination or amount vout.4 for pegsliquidate, it should be the rest of tokens to pegs CC global tokens address!");
                        else if (numvouts>5 && GetCCaddress(cp,addr,pegspk) && ConstrainVout(tx.vout[5],1,addr,0)==0)
                            return ("invalid coins destination or amount vout.5 for pegsliquidate, it should be change back to pegs CC global address!");
                        break;  
                }
            }
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Pegs tx validated\n");
            else fprintf(stderr,"Pegs tx invalid\n");
            return(retval);
        // }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPegsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk1,CPubKey pk2,int64_t total,int32_t maxinputs)
{
    // add threshold check
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    if (pk2.IsValid()) GetCCaddress1of2(cp,coinaddr,pk1,pk2);
    else GetCCaddress(cp,coinaddr,pk1);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        // no need to prevent dup
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if (myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                {
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    nValue = it->second.satoshis;
                    totalinputs += nValue;
                    n++;
                }
                if ( totalinputs >= total || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    return(totalinputs);
}

int64_t AddPegsTokenInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,uint256 pegstxid, uint256 tokenid, CPubKey pk1,CPubKey pk2, int64_t total,int32_t maxinputs)
{
    // add threshold check
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; uint256 tmppegstxid,tmptokenid; CPubKey mypk;

    if (pk2.IsValid()) GetTokensCCaddress1of2(cp,coinaddr,pk1,pk2);
    else GetTokensCCaddress(cp,coinaddr,pk1);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        // no need to prevent dup
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if (myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 && DecodePegsOpRet(vintx,tmppegstxid,tmptokenid)!=0 && tmppegstxid==pegstxid && tmptokenid==tokenid)
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    if (pk2.IsValid())
    {
        mypk = pubkey2pk(Mypubkey());
        if (mypk!=pk1 && mypk!=pk2)
        {
            CCaddrTokens1of2set(cp,pk1,pk2,cp->CCpriv,coinaddr);
        } 
        else
        {
            uint8_t mypriv[32];
            Myprivkey(mypriv);
            CCaddrTokens1of2set(cp,pk1,pk2,mypriv,coinaddr);
            memset(mypriv,0,sizeof(mypriv));
        }
    }
    return(totalinputs);
}

UniValue PegsCreate(const CPubKey& pk,uint64_t txfee,int64_t amount, std::vector<uint256> bindtxids)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());  std::vector<uint8_t> vorigpubkey;
    CPubKey mypk,pegspk; struct CCcontract_info *cp,C; CTransaction tx; int32_t numvouts; int64_t totalsupply; std::string coin,name,description;
    char depositaddr[64]; uint256 txid,hashBlock,tmptokenid,oracletxid; uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys;

    cp = CCinit(&C,EVAL_PEGS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (myGetTransaction(tmptokenid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find tokenid " << txid.GetHex());
        else if (DecodeTokenCreateOpRetV1(tx.vout[numvouts-1].scriptPubKey,vorigpubkey,name,description)!='c')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid token OP_RETURN data!");
        else if (!CheckSynthetic(description))
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid synthetic in token description field. You must put the price synthetic in token description field!");
    
    }                    
    if ( AddNormalinputs(mtx,mypk,amount+txfee,64,pk.IsValid()) >= amount+txfee )
    {
        for (int i=0; i<100; i++) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,(amount-txfee)/100,pegspk));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsCreateOpRet(bindtxids)));
    }
    CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "error adding normal inputs");
}

UniValue PegsFund(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string coin;
    CTransaction pegstx,tx; int32_t numvouts; int64_t totalsupply,balance=0,funds=0,tokenfunds=0; uint256 accounttxid=zeroid,hashBlock,txid,tmptokenid,oracletxid;
    CPubKey mypk,pegspk,tmppk; struct CCcontract_info *cp,*cpTokens,CTokens,C; char depositaddr[64],coinaddr[64]; std::pair <int64_t,int64_t> account(0,0);
    uint8_t M,N,taddr,prefix,prefix2,wiftype,mypriv[32]; std::vector<CPubKey> pubkeys; bool found=false; std::vector<uint256> bindtxids;

    cp = CCinit(&C,EVAL_PEGS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (tmptokenid==tokenid)
        {
            found=true;
            break;
        }
    }
    if (!found)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid tokenid " << tokenid.GetHex());
    if ((balance=GetTokenBalance(mypk,tokenid))>=amount)
    {
        PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,account);
        LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "current accounttxid=" << accounttxid.GetHex() << " [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
        if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
        if (accounttxid!=zeroid)
        {
            mtx.vin.push_back(CTxIn(accounttxid,0,CScript()));
            mtx.vin.push_back(CTxIn(accounttxid,1,CScript()));
            if (txfee>0 && (funds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee,1))<txfee)
                CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");      
            funds+=2*CC_MARKER_VALUE;
            Myprivkey(mypriv);
            GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
            CCaddr1of2set(cp,mypk,pegspk,mypriv,coinaddr);
            memset(mypriv,0,sizeof(mypriv));
        }
        else funds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee+2*CC_MARKER_VALUE,3);
        if (funds>=txfee+2*CC_MARKER_VALUE)
        {
            if ((tokenfunds=AddTokenCCInputs(cpTokens,mtx,mypk,tokenid,amount,64))>=amount)
            {
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,mypk,pegspk));
                mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_PEGS,amount,mypk,pegspk));
                if (tokenfunds-amount>0) mtx.vout.push_back(MakeTokensCC1vout(EVAL_TOKENS,tokenfunds-amount,mypk));
                if (funds>txfee+2*CC_MARKER_VALUE) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,funds-(txfee+2*CC_MARKER_VALUE),pegspk));
                account.first+=amount;
                LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "new account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);        
                return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsAccountOpRet('F',tokenid,pegstxid,mypk,amount,account,mypk)));
            }
            else CCERR_RESULT("pegscc",CCLOG_ERROR, stream <<"not enough balance of tokens in pegs global tokens CC address");
        }
        else CCERR_RESULT("pegscc",CCLOG_ERROR, stream <<"not enough balance in pegs global CC address");
    }
    else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance (" << balance << ") for this amount of tokens " << amount);
    return NullUniValue;
}

UniValue PegsGet(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t amount)
{
    CMutableTransaction burntx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()),mtx;
    CTransaction pegstx,tx; int32_t numvouts; int64_t funds=0; uint256 accounttxid=zeroid,hashBlock,pricestxid; char coinaddr[64];
    CPubKey mypk,pegspk,tmppk; struct CCcontract_info *cp,C; std::pair <int64_t,int64_t> account(0,0); uint8_t mypriv[32];
    std::vector<uint8_t> dummyproof; std::vector<CTxOut> vouts;  std::vector<uint256> bindtxids; CScript opret;

    cp = CCinit(&C,EVAL_PEGS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    if (PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,account)==0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account from which to issue coins, fund account first with pegsfund!");
    if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "current accounttxid=" << accounttxid.GetHex() << " [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
    // spending markers
    vouts.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));                
    vouts.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,mypk,pegspk));
    // coin issue
    vouts.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    account.second+=amount;
#ifndef TESTMODE_PEGS
    if (PegsGetRatio(tokenid,account)>PEGS_ACCOUNT_MAX_DEBT)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not possible to take more than " << PEGS_ACCOUNT_MAX_DEBT << "%% of the deposit");
#else
    if (PegsGetRatio(tokenid,account)>100)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not possible to take more than 100%% of the deposit");
#endif
    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "new account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
    // burn tx does not exist in pegs method but it must be created in order for import validation to pass
    // fictive burntx input of previous account state tx
    burntx.vin.push_back(CTxIn(accounttxid,0,CScript()));
    // fictive output of coins in burn tx
    burntx.vout.push_back(MakeBurnOutput(amount,0xffffffff,"PEGSCC",vouts,dummyproof,pegstxid,tokenid,mypk,amount,account,mypk));
    std::vector<uint256> leaftxids;
    BitcoinGetProofMerkleRoot(dummyproof, leaftxids);
    MerkleBranch newBranch(0, leaftxids);
    TxProof txProof = std::make_pair(burntx.GetHash(), newBranch);
    mtx=MakePegsImportCoinTransaction(txProof,burntx,vouts);
    Myprivkey(mypriv);
    GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
    CCaddr1of2set(cp,mypk,pegspk,mypriv,coinaddr);
    UniValue retstr = FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,opret);
    memset(mypriv,0,sizeof(mypriv));
    return(retstr);
}

UniValue PegsRedeem(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t tokenamount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string coin;
    CTransaction pegstx,tx; int32_t numvouts; int64_t totalsupply,pegsfunds=0,funds=0,tokenfunds=0; uint256 accounttxid=zeroid,hashBlock,txid,tmptokenid,oracletxid;
    CPubKey mypk,pegspk,tmppk; struct CCcontract_info *cp,*cpTokens,CTokens,C; char depositaddr[64],coinaddr[64]; std::pair <int64_t,int64_t> account(0,0);
    uint8_t M,N,taddr,prefix,prefix2,wiftype,mypriv[32]; std::vector<CPubKey> pubkeys; bool found=false; std::vector<uint256> bindtxids;

    cp = CCinit(&C,EVAL_PEGS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (tmptokenid==tokenid)
        {
            found=true;
            break;
        }
    }
    if (!found)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid tokenid " << tokenid.GetHex());
    if (PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,account)==0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account from which to redeem tokens!");
    if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
    if (PegsGetRatio(tokenid,account)>=PEGS_ACCOUNT_MAX_DEBT)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot redeem when account ratio >= " << PEGS_ACCOUNT_MAX_DEBT << "%%");
    if (tokenamount>account.first-(PegsGetTokensAmountPerPrice(account.second,tokenid)*100/PEGS_ACCOUNT_MAX_DEBT))
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot redeem this amount of tokens, you must leave enough tokens to leave account ratio <= " << PEGS_ACCOUNT_MAX_DEBT << "%%");
    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "current accounttxid=" << accounttxid.GetHex() << " [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
    mtx.vin.push_back(CTxIn(accounttxid,0,CScript()));
    mtx.vin.push_back(CTxIn(accounttxid,1,CScript()));
    if (txfee>0 && (pegsfunds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee,1))<txfee)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");      
    pegsfunds+=2*CC_MARKER_VALUE;
    Myprivkey(mypriv);
    GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
    CCaddr1of2set(cp,mypk,pegspk,mypriv,coinaddr);
    memset(mypriv,0,32);
    account.first-=tokenamount;
    if ((tokenfunds=AddPegsTokenInputs(cp,mtx,pegstxid,tokenid,mypk,pegspk,tokenamount,64))>=tokenamount)
    {      
        mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));
        mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,mypk,pegspk));
        mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,tokenamount,mypk));
        if (tokenfunds>tokenamount) mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_PEGS,tokenfunds-tokenamount,mypk,pegspk));
        if (pegsfunds>txfee+2*CC_MARKER_VALUE) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,pegsfunds-(txfee+2*CC_MARKER_VALUE),pegspk));                    
        LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "new account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
        UniValue retstr = FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsAccountOpRet('R',tokenid,pegstxid,mypk,tokenamount,account,mypk));
        return(retstr);
    }
    else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough tokens in pegs account (" << tokenfunds << ") to redeem this amount of tokens " << tokenamount);
}

UniValue PegsClose(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string coin;
    CTransaction pegstx,tx; int32_t numvouts; int64_t totalsupply,pegsfunds=0,funds=0,tokenfunds=0,tokenamount,burnamount; uint256 accounttxid=zeroid,hashBlock,txid,tmptokenid,oracletxid;
    CPubKey mypk,pegspk,tmppk; struct CCcontract_info *cp,*cpTokens,CTokens,C; char depositaddr[64],coinaddr[64]; std::pair <int64_t,int64_t> account(0,0);
    uint8_t M,N,taddr,prefix,prefix2,wiftype,mypriv[32]; std::vector<CPubKey> pubkeys; bool found=false; std::vector<uint256> bindtxids;

    cp = CCinit(&C,EVAL_PEGS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (tmptokenid==tokenid)
        {
            found=true;
            break;
        }
    }
    if (!found)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid tokenid " << tokenid.GetHex());
    if (PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,account)==0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account to close!");
    if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "current accounttxid=" << accounttxid.GetHex() << " [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
    mtx.vin.push_back(CTxIn(accounttxid,0,CScript()));
    mtx.vin.push_back(CTxIn(accounttxid,1,CScript()));
    Myprivkey(mypriv);
    GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
    CCaddr1of2set(cp,mypk,pegspk,mypriv,coinaddr);
    memset(mypriv,0,32);
    if ((funds=AddNormalinputs(mtx,mypk,account.second,64,pk.IsValid()))>=account.second )
    { 
        if ((pegsfunds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee,1))>=txfee)
        {
            pegsfunds+=2*CC_MARKER_VALUE;
            tokenamount=account.first;
            burnamount=account.second;
            if ((tokenfunds=AddPegsTokenInputs(cp,mtx,pegstxid,tokenid,mypk,pegspk,tokenamount,64))>=tokenamount)
            {   
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,mypk,pegspk));
                mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,tokenamount,mypk));
                mtx.vout.push_back(CTxOut(account.second,CScript() << ParseHex(HexStr(CCtxidaddr(coinaddr,pegstxid))) << OP_CHECKSIG));
                if (pegsfunds>txfee+2*CC_MARKER_VALUE) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,pegsfunds-(txfee+2*CC_MARKER_VALUE),pegspk));
                account.first=0;
                account.second=0;
                LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "new account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
                UniValue retstr = FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsAccountOpRet('X',tokenid,pegstxid,mypk,burnamount,account,mypk));
                return(retstr);
            }     
            else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough tokens in pegs account (" << tokenfunds << ") to take this amount of tokens " << account.first);
        }
        else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");       
    }
    else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "to close your account you must return full debt amount " << account.second << " instead of " << funds);
}

UniValue PegsExchange(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string coin;
    CTransaction pegstx,tx; int32_t numvouts; int64_t totalsupply,pegsfunds=0,funds=0,tokenfunds=0,tokenamount,tmpamount; uint256 accounttxid=zeroid,hashBlock,txid,tmptokenid,oracletxid;
    CPubKey mypk,pegspk,tmppk,accountpk; struct CCcontract_info *cp,*cpTokens,CTokens,C; char depositaddr[64],coinaddr[64]; std::pair <int64_t,int64_t> account(0,0);
    uint8_t M,N,taddr,prefix,prefix2,wiftype,mypriv[32]; std::vector<CPubKey> pubkeys; bool found=false; std::vector<uint256> bindtxids;

    cp = CCinit(&C,EVAL_PEGS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (tmptokenid==tokenid)
        {
            found=true;
            break;
        }
    }
    if (!found)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid tokenid " << tokenid.GetHex());
    if (PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,account)!=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "you have active account, please close account first before exchanging other coins!");
    if ((funds=AddNormalinputs(mtx,mypk,amount,64,pk.IsValid()))>=amount )
    { 
        if ((pegsfunds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee,1))>=txfee)
        {
            tokenamount=PegsGetTokensAmountPerPrice(amount,tokenid); 
            tokenfunds=AddPegsTokenInputs(cp,mtx,pegstxid,tokenid,pegspk,CPubKey(),tokenamount,64);
            if (tokenfunds<tokenamount)
            {
                if (PegsFindSuitableAccount(cp,pegstxid,tokenid,tokenamount-tokenfunds,accounttxid,account).empty())
                    CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account from which to get tokens for exchange!");
                if (accounttxid!=zeroid && myGetTransaction(accounttxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0 || PegsDecodeAccountTx(tx,tmppk,tmpamount,account,accountpk).empty())
                    CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid account tx from which to exchange coins to tokens " << accounttxid.GetHex());
                if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
                    CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
                tokenfunds+=AddPegsTokenInputs(cp,mtx,pegstxid,tokenid,accountpk,pegspk,tokenamount,64);
                mtx.vin.insert(mtx.vin.begin(),CTxIn(accounttxid,0,CScript()));
                mtx.vin.insert(mtx.vin.begin(),CTxIn(accounttxid,1,CScript()));
                GetCCaddress1of2(cp,coinaddr,accountpk,pegspk);
                CCaddr1of2set(cp,accountpk,pegspk,cp->CCpriv,coinaddr);
                pegsfunds+=2*CC_MARKER_VALUE;
            }
            if (tokenfunds>=tokenamount)
            {
                if (accounttxid!=zeroid)
                {        
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,accountpk,pegspk));
                }
                if ((accounttxid!=zeroid && pegsfunds>=txfee+2*CC_MARKER_VALUE) || pegsfunds>=txfee)
                {
                    mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,tokenamount,mypk));
                    mtx.vout.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(CCtxidaddr(coinaddr,pegstxid))) << OP_CHECKSIG));
                    if (tokenfunds>tokenamount) mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_PEGS,tokenfunds-tokenamount,accountpk,pegspk));
                    if (accounttxid!=zeroid)
                    {
                        if (pegsfunds>txfee+2*CC_MARKER_VALUE) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,pegsfunds-(txfee+2*CC_MARKER_VALUE),pegspk));
                        account.first=account.first-tokenamount;
                        account.second=account.second-amount;
                    }
                    else if (pegsfunds>txfee) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,pegsfunds-txfee,pegspk));
                    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "modified account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
                    return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsAccountOpRet('E',tokenid,pegstxid,mypk,amount,account,accountpk)));
                }
                else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");
            }
            else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough tokens in pegs account (" << tokenfunds << ") to exchange to this amount of tokens " << tokenamount);
        }
        else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");
    }
    else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough funds to exchange " << amount << " coins to tokens - balance " << funds);
}

UniValue PegsLiquidate(const CPubKey& pk,uint64_t txfee,uint256 pegstxid, uint256 tokenid, uint256 liquidatetxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()); std::string coin;
    CTransaction pegstx,tx; int32_t numvouts; int64_t totalsupply,pegsfunds=0,funds=0,tokenfunds=0,amount,tmpamount,tokenamount,burnamount;
    CPubKey mypk,pegspk,tmppk,accountpk; struct CCcontract_info *cp,*cpTokens,CTokens,C; char depositaddr[64],coinaddr[64]; std::pair <int64_t,int64_t> account(0,0),myaccount(0,0);
    uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys; bool found=false; std::vector<uint256> bindtxids;
    uint256 hashBlock,txid,tmptokenid,oracletxid,accounttxid;

    cp = CCinit(&C,EVAL_PEGS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_PEGS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
   for(auto txid : bindtxids)
    {
        if (myGetTransaction(txid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find bindtxid " << txid.GetHex());
        if (DecodeGatewaysBindOpRet(depositaddr,tx.vout[numvouts-1].scriptPubKey,tmptokenid,coin,totalsupply,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)!='B')
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid bindtxid " << txid.GetHex());
        if (tmptokenid==tokenid)
        {
            found=true;
            break;
        }
    }
    if (!found)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid tokenid " << tokenid.GetHex());
    if (PegsFindAccount(cp,mypk,pegstxid,tokenid,accounttxid,myaccount)==0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account, you must have an account to liquidate another account!");
    if (PegsGetRatio(tokenid,myaccount)>=(ASSETCHAINS_PEGSCCPARAMS[0]?ASSETCHAINS_PEGSCCPARAMS[0]:PEGS_ACCOUNT_RED_ZONE))
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not able to liquidate another account when your account ratio is in red zone - ratio > " << (ASSETCHAINS_PEGSCCPARAMS[0]?ASSETCHAINS_PEGSCCPARAMS[0]:PEGS_ACCOUNT_RED_ZONE) << "%%");
    if (accounttxid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,accounttxid,1) != 0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous account tx not yet confirmed");
    if (liquidatetxid==zeroid || myGetTransaction(liquidatetxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0 || PegsDecodeAccountTx(tx,tmppk,amount,account,accountpk).empty())
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cannot find account to liquidate or invalid tx " << liquidatetxid.GetHex());
    if (PegsGetAccountRatio(pegstxid,tokenid,liquidatetxid)<(ASSETCHAINS_PEGSCCPARAMS[0]?ASSETCHAINS_PEGSCCPARAMS[0]:PEGS_ACCOUNT_RED_ZONE) || PegsGetGlobalRatio(pegstxid)<(ASSETCHAINS_PEGSCCPARAMS[1]?ASSETCHAINS_PEGSCCPARAMS[1]:PEGS_GLOBAL_RED_ZONE))
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not able to liquidate account until account ratio >= " << (ASSETCHAINS_PEGSCCPARAMS[0]?ASSETCHAINS_PEGSCCPARAMS[0]:PEGS_ACCOUNT_RED_ZONE) << "%% and global ratio >= " << (ASSETCHAINS_PEGSCCPARAMS[1]?ASSETCHAINS_PEGSCCPARAMS[1]:PEGS_GLOBAL_RED_ZONE) << "%%");
    if (myIsutxo_spentinmempool(ignoretxid,ignorevin,liquidatetxid,1) != 0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "previous liquidation account tx not yet confirmed");
    LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "current accounttxid=" << accounttxid.GetHex() << " [deposit=" << myaccount.first << ",debt=" << myaccount.second << "]" << std::endl);
    tokenamount=account.first;
    burnamount=account.second;
    tmpamount=PegsGetTokensAmountPerPrice(burnamount,tokenid)*105/100;
    amount=tmpamount+((tokenamount-tmpamount)*10/100);
    mtx.vin.push_back(CTxIn(liquidatetxid,0,CScript()));
    mtx.vin.push_back(CTxIn(liquidatetxid,1,CScript()));
    if ((funds=AddNormalinputs(mtx,mypk,account.second,64))>=burnamount)
    { 
        if ((pegsfunds=AddPegsInputs(cp,mtx,pegspk,CPubKey(),txfee,1))<txfee)
            CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");  
        pegsfunds+=2*CC_MARKER_VALUE;
        GetCCaddress1of2(cp,coinaddr,accountpk,pegspk);
        CCaddr1of2set(cp,accountpk,pegspk,cp->CCpriv,coinaddr);            
        if ((tokenfunds=AddPegsTokenInputs(cp,mtx,pegstxid,tokenid,accountpk,pegspk,tokenamount,64))==tokenamount)
        {
            if (pegsfunds>=txfee+2*CC_MARKER_VALUE)
            {        
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,pegspk,pegspk));
                mtx.vout.push_back(MakeCC1of2vout(EVAL_PEGS,CC_MARKER_VALUE,accountpk,pegspk));
                mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,amount,mypk));
                mtx.vout.push_back(CTxOut(burnamount,CScript() << ParseHex(HexStr(CCtxidaddr(coinaddr,pegstxid))) << OP_CHECKSIG));
                mtx.vout.push_back(MakeTokensCC1vout(EVAL_PEGS,tokenamount-amount,pegspk));
                if (pegsfunds>txfee+2*CC_MARKER_VALUE) mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,pegsfunds-(txfee+2*CC_MARKER_VALUE),pegspk));
                account.first=0;
                account.second=0;
                LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "new account [deposit=" << account.first << ",debt=" << account.second << "]" << std::endl);
                return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodePegsAccountOpRet('L',tokenid,pegstxid,mypk,burnamount,account,accountpk)));
            }
            else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough balance in pegs global CC address");
        }
        else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "tokens amount in pegs account " << tokenfunds << " not matching amount in account " << tokenamount); // this shouldn't happen
    }
    else CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "not enough funds to liquidate account, you must liquidate full debt ammount " << txfee+account.second << " instead of " << funds);
    
}

UniValue PegsAccountHistory(const CPubKey& pk,uint256 pegstxid)
{
    char coinaddr[64]; int64_t nValue,amount; uint256 txid,accounttxid,hashBlock,tmptokenid,tmppegstxid;
    CTransaction tx; int32_t numvouts,vout; char funcid; CPubKey mypk,pegspk,tmppk,accountpk; std::map<uint256,std::pair<int64_t,int64_t>> accounts;
    std::vector<uint256> txids; std::pair<int64_t,int64_t> account; std::vector<uint256> bindtxids;
    UniValue result(UniValue::VOBJ),acc(UniValue::VARR); struct CCcontract_info *cp,C;

    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","pegsaccounthistory"));
    cp = CCinit(&C,EVAL_PEGS);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
    SetCCtxids(txids,coinaddr,true,EVAL_PEGS,CC_MARKER_VALUE,pegstxid,0);
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if (myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (funcid=DecodePegsOpRet(tx,tmppegstxid,tmptokenid))!=0 && pegstxid==tmppegstxid)
        {
            UniValue obj(UniValue::VOBJ);
            obj.push_back(Pair("action",PegsDecodeAccountTx(tx,tmppk,amount,account,accountpk)));
            obj.push_back(Pair("amount",amount));
            obj.push_back(Pair("accounttxid",txid.GetHex()));
            obj.push_back(Pair("token",PegsGetTokenName(tmptokenid)));                
            obj.push_back(Pair("deposit",account.first));
            obj.push_back(Pair("debt",account.second));
            acc.push_back(obj);         
        }
    }
    result.push_back(Pair("account history",acc));    
    return(result);
}

UniValue PegsAccountInfo(const CPubKey& pk,uint256 pegstxid)
{
    char coinaddr[64]; int64_t nValue,amount; uint256 txid,accounttxid,hashBlock,tmptokenid,tmppegstxid; std::map<uint256,std::pair<int64_t,int64_t>> accounts;
    CTransaction tx; int32_t numvouts,vout; char funcid; CPubKey mypk,pegspk,tmppk,accountpk; std::vector<uint256> bindtxids;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; std::pair<int64_t,int64_t> account; 
    UniValue result(UniValue::VOBJ),acc(UniValue::VARR); struct CCcontract_info *cp,C;

    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","pegsaccountinfo"));
    cp = CCinit(&C,EVAL_PEGS);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,mypk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        //LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "txid=" << txid.GetHex() << ", vout=" << vout << ", nValue=" << nValue << std::endl);
        if (vout == 1 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (funcid=DecodePegsOpRet(tx,tmppegstxid,tmptokenid))!=0 && pegstxid==tmppegstxid)
        {
            //LOGSTREAM("pegscc",CCLOG_DEBUG2, stream << "txid=" << txid.GetHex() << ", vout=" << vout << ", nValue=" << nValue << ", tokenid=" << tmptokenid.GetHex() << std::endl);
            PegsDecodeAccountTx(tx,tmppk,amount,account,accountpk);           
            accounts[tmptokenid].first=account.first;
            accounts[tmptokenid].second=account.second;
        }
    }
    for (std::map<uint256,std::pair<int64_t,int64_t>>::iterator it = accounts.begin(); it != accounts.end(); ++it)
    {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("token",PegsGetTokenName(it->first)));
        obj.push_back(Pair("price",(double)PegsGetTokenPrice(it->first)/COIN));
        obj.push_back(Pair("deposit",accounts[it->first].first));
        obj.push_back(Pair("debt",accounts[it->first].second));
        if (accounts[it->first].first==0 || accounts[it->first].second==0 || PegsGetTokenPrice(it->first)<=0) obj.push_back(Pair("ratio",0));
        else obj.push_back(Pair("ratio",strprintf("%.2f%%",PegsGetRatio(it->first,accounts[it->first]))));         
        acc.push_back(obj);
    }
    result.push_back(Pair("account info",acc));    
    return(result);
}

UniValue PegsWorstAccounts(uint256 pegstxid)
{
    char coinaddr[64]; int64_t nValue,amount; uint256 txid,accounttxid,hashBlock,tmppegstxid,tokenid,prev;
    CTransaction tx; int32_t numvouts,vout; char funcid; CPubKey pegspk,pk,accountpk; double ratio; std::vector<uint256> bindtxids;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; std::pair<int64_t,int64_t> account;
    UniValue result(UniValue::VOBJ),acc(UniValue::VARR); struct CCcontract_info *cp,C; std::multimap<uint256,UniValue> map;

    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","pegsworstaccounts"));
    cp = CCinit(&C,EVAL_PEGS);
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,pegspk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if (vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (funcid=DecodePegsOpRet(tx,tmppegstxid,tokenid))!=0 && pegstxid==tmppegstxid)
        {               
            PegsDecodeAccountTx(tx,pk,amount,account,accountpk);
            if (account.first==0 || account.second==0 || PegsGetTokenPrice(tokenid)<=0) ratio=0;
            else ratio=PegsGetRatio(tokenid,account);
            if (ratio>PEGS_ACCOUNT_RED_ZONE)
            {
                UniValue obj(UniValue::VOBJ);
                obj.push_back(Pair("accounttxid",txid.GetHex()));
                obj.push_back(Pair("deposit",account.first));
                obj.push_back(Pair("debt",account.second));
                obj.push_back(Pair("ratio",strprintf("%.2f%%",ratio)));                
                map.insert(std::pair<uint256,UniValue>(tokenid,obj));
            }
        }
    }
    std::multimap<uint256,UniValue>::iterator it = map.begin();
    for (prev=it->first; it != map.end(); ++it)
    {
        if (it->first!=prev)
        {
            result.push_back(Pair(PegsGetTokenName(prev),acc));
            acc.clear();
            prev=it->first;
        }
        acc.push_back(it->second);
    }
    result.push_back(Pair(PegsGetTokenName(prev),acc));
    return(result);
}

UniValue PegsInfo(uint256 pegstxid)
{
    char coinaddr[64]; int64_t nValue,amount; uint256 txid,accounttxid,hashBlock,tmppegstxid,tokenid;
    CTransaction tx; int32_t numvouts,vout; char funcid; CPubKey pegspk,pk,accountpk; std::vector<uint256> bindtxids;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; std::pair<int64_t,int64_t> account;
    std::map<uint256,std::pair<int64_t,int64_t>> globalaccounts; double globaldeposit=0;
    UniValue result(UniValue::VOBJ),gateways(UniValue::VARR),acc(UniValue::VARR); struct CCcontract_info *cp,C;

    if (KOMODO_EARLYTXID!=zeroid && pegstxid!=KOMODO_EARLYTXID)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid. On this chain only valid pegstxid is " << KOMODO_EARLYTXID.GetHex());
    if (myGetTransaction(pegstxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "cant find pegstxid " << pegstxid.GetHex());
    if (DecodePegsCreateOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxids)!='C')
        CCERR_RESULT("pegscc",CCLOG_ERROR, stream << "invalid pegstxid " << pegstxid.GetHex());
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","pegsinfo"));
    for (std::vector<uint256>::const_iterator it=bindtxids.begin(); it!=bindtxids.end(); it++)
    {
        gateways.push_back(it->GetHex());
    }
    result.push_back(Pair("gateways",gateways));
    cp = CCinit(&C,EVAL_PEGS);
    pegspk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,pegspk,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if (vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 &&
            (funcid=DecodePegsOpRet(tx,tmppegstxid,tokenid))!=0 && pegstxid==tmppegstxid)
        {               
            PegsDecodeAccountTx(tx,pk,amount,account,accountpk);
            globalaccounts[tokenid].first+=account.first;
            globalaccounts[tokenid].second+=account.second;
        }
    }
    unspentOutputs.clear();
    GetTokensCCaddress(cp,coinaddr,pegspk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if (myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size())>0 && DecodePegsOpRet(tx,tmppegstxid,tokenid)!=0 && pegstxid==tmppegstxid)
        {
            globalaccounts[tokenid].first+=nValue;
        }
    }
    for (std::map<uint256,std::pair<int64_t,int64_t>>::iterator it = globalaccounts.begin(); it != globalaccounts.end(); ++it)
    {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("token",PegsGetTokenName(it->first)));
        obj.push_back(Pair("price",(double)PegsGetTokenPrice(it->first)/COIN));
        obj.push_back(Pair("total deposit",globalaccounts[it->first].first));
        obj.push_back(Pair("total debt",globalaccounts[it->first].second));
        if (globalaccounts[it->first].first==0 || globalaccounts[it->first].second==0 || PegsGetTokenPrice(it->first)<=0) obj.push_back(Pair("total ratio",0));
        else obj.push_back(Pair("total ratio",strprintf("%.2f%%",PegsGetRatio(it->first,globalaccounts[it->first]))));                
        acc.push_back(obj);
    }
    result.push_back(Pair("info",acc));
    result.push_back(Pair("global ratio",strprintf("%.2f%%",PegsGetGlobalRatio(pegstxid))));
    return(result);
}
