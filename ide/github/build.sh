echo "Check git variables"
echo "CurrDir = $(pwd)"
# echo "github.workspace = ${{ github.workspace }}"
echo "GITHUB_WORKSPACE = $GITHUB_WORKSPACE"
echo "GITHUB_ENV = $GITHUB_ENV"
echo "--------------------"
echo "cat $GITHUB_ENV"
## cat ${GITHUB_ENV}
echo "--------------------"
echo "GITHUB_REPO = $GITHUB_REPOSITORY"
echo "GITHUB_SHA = $GITHUB_SHA"
## echo ${GITHUB_SHA:0:7}
## echo .
# Bad substitution: GIT_HASH=$(echo ${GITHUB_SHA:0:7} )

GIT_HASH=$(echo $GITHUB_SHA | head -c 7)
echo "GIT_HASH=$GIT_HASH" >> $GITHUB_ENV
echo "GIT_HASH = $GIT_HASH"
export GIT_HASH=$GIT_HASH



##          echo "CurrDir = $(pwd)"
##          ls -l .git/
##          chmod +x ./ide/github/build.sh
##          ./ide/github/build.sh
##          # echo "GIT_HASH=$GIT_HASH" >> $GITHUB_ENV
##          echo "GIT_HASH(2) = $GIT_HASH"

##          echo "GIT_HASH = $GIT_HASH"
##          echo "GIT_HASH(3) = ${{ env.GIT_HASH }} "
##        working-directory: ${{ github.workspace }}


          ####    echo "Check git variables"
          ####    echo "CurrDir = $(pwd)"
          ####    echo "github.workspace = ${{ github.workspace }}"
          ####    echo "GITHUB_WORKSPACE = $GITHUB_WORKSPACE"
          ####    echo "GITHUB_ENV = $GITHUB_ENV"
          ####    echo "--------------------"
          ####    echo "cat $GITHUB_ENV"
          ####    ## cat ${GITHUB_ENV}
          ####    echo "--------------------"
          ####    echo "GITHUB_REPO = $GITHUB_REPOSITORY"
          ####    echo "GITHUB_SHA = $GITHUB_SHA"
          ####    ## echo ${GITHUB_SHA:0:7}
          ####    ## echo .
          ####    # Bad substitution: GIT_HASH=$(echo ${GITHUB_SHA:0:7} )
          ####    GIT_HASH=$(echo $GITHUB_SHA | head -c 7)
          ####    echo "GIT_HASH = $GIT_HASH"

          ### echo "GIT_HASH = $GIT_SHA"
          ### GIT_HASH=${GITHUB_SHA}
          ### echo "GIT_HASH = $GIT_HASH"
          ### echo "Test:= ${{ github.workspace }}"
          ### echo $GITHUB_SHA | head -c 10
          ### echo .
          ### GIT_HASH=$($GITHUB_SHA | head -c 10)
          ### echo "GIT_HASH = $GIT_HASH"
          ### GIT_HASH=$(echo $GITHUB_SHA | head -c 10)
          ### echo "GIT_HASH = $GIT_HASH"
          ### # echo Test:= $GITHUB_SHA | head -c 10
          ### # GIT_HASH=$(echo  ${{ github.GITHUB_SHA }} | head -c 10)
          ### echo "GIT_HASH = $GIT_HASH"
          ### ###  echo "GIT_HASH = $GIT_HASH"
          ### ###  ### if [ -z '${{ env.git_hash }}' ]; then
          ### ###  ###   echo "git_hash isn't set!!!"
          ### ###  ###   git_hash=$(git rev-parse --short "$GITHUB_SHA")
          ### ###  ###   echo 'git_hash = $(git rev-parse --short "$GITHUB_SHA")' >> $GITHUB_ENV
          ### ###  ###   echo "git_hash = ${{ env.git_hash }}"
          ### ###  ### else
          ### ###  ###   echo "git_hash = ${{ env.git_hash }}"
          ### ###  ###   echo "git_hash = $git_hash"
          ### ###  ### fi
