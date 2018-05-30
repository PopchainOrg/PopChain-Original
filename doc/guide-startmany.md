#start-many Setup Guide

## Two Options for Setting up your Wallet
There are many ways to setup a wallet to support start-many. This guide will walk through two of them.

1. [Importing an existing wallet (recommended if you are consolidating wallets).](#option1)
2. [Sending 10000 UC to new wallet addresses.](#option2)

## <a name="option1"></a>Option 1. Importing an existing wallet

This is the way to go if you are consolidating multiple wallets into one that supports start-many. 

### From your single-instance Popnode Wallet

Open your QT Wallet and go to console (from the menu select `Tools` => `Debug Console`)

Dump the private key from your PopNode's pulic key.

```
walletpassphrase [your_wallet_passphrase] 600
dumpprivkey [pn_public_key]
```

Copy the resulting priviate key. You'll use it in the next step.

### From your multi-instance Popnode Wallet

Open your QT Wallet and go to console (from the menu select `Tools` => `Debug Console`)

Import the private key from the step above.

```
walletpassphrase [your_wallet_passphrase] 600
importprivkey [single_instance_private_key]
```

The wallet will re-scan and you will see your available balance increase by the amount that was in the imported wallet.

[Skip Option 2. and go to Create popnode.conf file](#popnodeconf)

## <a name="option2"></a>Option 2. Starting with a new wallet

[If you used Option 1 above, then you can skip down to Create popnode.conf file.](#popnodeconf)

### Create New Wallet Addresses

1. Open the QT Wallet.
2. Click the Receive tab.
3. Fill in the form to request a payment.
    * Label: pn01
    * Amount: 10000 (optional)
    * Click *Request payment* button
5. Click the *Copy Address* button

Create a new wallet address for each Popnode.

Close your QT Wallet.

### Send 10000 UC to New Addresses

Just like setting up a standard PN. Send exactly 10000 UC to each new address created above.

### Create New Popnode Private Keys

Open your QT Wallet and go to console (from the menu select `Tools` => `Debug Console`)

Issue the following:

```popnode genkey```

*Note: A popnode private key will need to be created for each Popnode you run. You should not use the same popnode private key for multiple Popnodes.*

Close your QT Wallet.

## <a name="popnodeconf"></a>Create popnode.conf file

Remember... this is local. Make sure your QT is not running.

Create the `popnode.conf` file in the same directory as your `wallet.dat`.

Copy the popnode private key and correspondig collateral output transaction that holds the 10000 UC.

The popnode private key may be an existing key from [Option 1](#option1), or a newly generated key from [Option 2](#option2). 

*Note: The popnode priviate key is **not** the same as a wallet private key. **Never** put your wallet private key in the popnode.conf file. That is almost equivalent to putting your 10000 UC on the remote server and defeats the purpose of a hot/cold setup.*

### Get the collateral output

Open your QT Wallet and go to console (from the menu select `Tools` => `Debug Console`)

Issue the following:

```popnode outputs```

Make note of the hash (which is your collateral_output) and index.

### Enter your Popnode details into your popnode.conf file
[From the pop github repo](https://github.com/poppay/pop/blob/master/doc/popnode_conf.md)

`popnode.conf` format is a space seperated text file. Each line consisting of an alias, IP address followed by port, popnode private key, collateral output transaction id and collateral output index.

```
alias ipaddress:port popnode_private_key collateral_output collateral_output_index
```

Example:

```
pn01 127.0.0.1:9888 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0
pn02 127.0.0.2:9888 93WaAb3htPJEV8E9aQcN23Jt97bPex7YvWfgMDTUdWJvzmrMqey aa9f1034d973377a5e733272c3d0eced1de22555ad45d6b24abadff8087948d4 0
```

## What about the pop.conf file?

If you are using a `popnode.conf` file you no longer need the `pop.conf` file. The exception is if you need custom settings (_thanks oblox_). In that case you **must** remove `popnode=1` from local `pop.conf` file. This option should be used only to start local Hot popnode now.

## Update pop.conf on server

If you generated a new popnode private key, you will need to update the remote `pop.conf` files.

Shut down the daemon and then edit the file.

```nano .popcore/pop.conf```

### Edit the popnodeprivkey
If you generated a new popnode private key, you will need to update the `popnodeprivkey` value in your remote `pop.conf` file.

## Start your Popnodes

### Remote

If your remote server is not running, start your remote daemon as you normally would. 

You can confirm that remote server is on the correct block by issuing

```pop-cli getinfo```

and comparing with the official explorer at https://explorer.pop.org/chain/Pop

### Local

Finally... time to start from local.

#### Open up your QT Wallet

From the menu select `Tools` => `Debug Console`

If you want to review your `popnode.conf` setting before starting Popnodes, issue the following in the Debug Console:

```popnode list-conf```

Give it the eye-ball test. If satisfied, you can start your Popnodes one of two ways.

1. `popnode start-alias [alias_from_popnode.conf]`  
Example ```popnode start-alias pn01```
2. `popnode start-many`

## Verify that Popnodes actually started

### Remote

Issue command `popnode status`
It should return you something like that:
```
pop-cli popnode status
{
    "vin" : "CTxIn(COutPoint(<collateral_output>, <collateral_output_index>), scriptSig=)",
    "service" : "<ipaddress>:<port>",
    "pubkey" : "<10000 UC address>",
    "status" : "Popnode successfully started"
}
```
Command output should have "_Popnode successfully started_" in its `status` field now. If it says "_not capable_" instead, you should check your config again.

### Local

Search your Popnodes on https://popninja.pl/popnodes.html

_Hint: Bookmark it, you definitely will be using this site a lot._
